/*
 *  Dump Tool
 *
 *  See README.md
 *
 *  Copyright (C) 2015 Timo J. Rinne <tri@iki.fi>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include "common.h"

const char *av0 = NULL;

int main(int argc, char **argv) {
  const char *fn = NULL, *fn2 = NULL;
  struct stat st;
  int fd, fd2;
  char *buf = NULL;
  uint64_t srcsize = 0, nblocks = 0, block = 0, nbytes = 0;
  const char *hlp;
  dump_stump_t stump = NULL, tstump = NULL;
  double t0, t1;
  int c;

  (void)ts();

  /* Arguments */
  hlp = strrchr(argv[0], '/');
  if (hlp == NULL)
    av0 = argv[0];
  else
    av0 = hlp + 1;
  opterr = 0;
  while ((c = getopt(argc, argv, "+S:")) > 0) {
      switch (c) {
      case 'S':
	tstump = dump_stump_parse(optarg);
	if (tstump == NULL) {
	  dprintf(2, "%s: Invalid stump spec in option '%c'. Must be <offset>:<string>\n", av0, optopt);
	  dprintf(2, "Usage: %s <source device or file> <destination-file>\n", av0);
	  fsync(2);
	  exit(1);
	}
	tstump->next = stump;
	stump = tstump;
	tstump = NULL;
	break;
      default:
	dprintf(2, "%s: Invalid option '%c'.\n", av0, optopt);
	dprintf(2, "Usage: %s <source device or file> <destination-file>\n", av0);
	fsync(2);
	exit(1);
      }
  }
  argc -= optind;
  argv += optind;
  if (argc == 2) {
    fn = argv[0];
    fn2 = argv[1];
  } else {
    dprintf(2, "Usage: %s <source device or file> <destination-file>\n", av0);
    fsync(2);
    exit(1);
 }
  if (lstat(fn, &st) != 0) {
    dprintf(2, "Can't stat source \"%s\"\n", fn);
    fsync(2);
    exit(1);
  }

  /* Input type check */
  if (S_ISREG(st.st_mode)) {
    srcsize = st.st_size;
    dprintf(1, "Input is a regular file.\n");
    fsync(1);
  } else if (S_ISCHR(st.st_mode)) {
    srcsize = st.st_size;
    dprintf(1, "Input is a character device.\n");
    fsync(1);
  } else if (S_ISBLK(st.st_mode)) {
    if (((fd = open(fn, O_RDONLY | O_NOFOLLOW, 0)) < 0) || (ioctl(fd, BLKGETSIZE64, &srcsize) != 0) || (close(fd) != 0)) {
      dprintf(2, "Can't get size of the block device \"%s\"\n", fn);
      fsync(2);
      exit(1);
    }
    dprintf(1, "Input is a block device.\n");
    fsync(1);
  } else {
    dprintf(2, "Input \"%s\" is not regular file or device node\n", fn);
    fsync(2);
    exit(1);
  }

  /* Size calculation */
  dprintf(1, "Source size is %lu bytes.\n", (unsigned long)srcsize);
  fsync(1);
  if (srcsize < 1) {
    dprintf(2, "Cowardly declining the creation of empty dump.\n");
    fsync(2);
    exit(1);
  }
  if ((srcsize / 1048576) > 1048576) {
    dprintf(2, "Cowardly declining the creation of dump size of over 1 terabyte.\n");
    fsync(2);
    exit(1);
  }
  nblocks = srcsize / FBLOCK_LEN;
  if ((nblocks * FBLOCK_LEN) < srcsize) {
    nblocks++;
  }
  dprintf(1, "Dumpfile will have %lu blocks.\n", (unsigned long)nblocks);
  fsync(1);

  /* Buffer */
  if (! (buf = malloc(FBUF_LEN))) {
    dprintf(2, "Out of memory\n");
    fsync(2);
    exit(1);
  }

  /* Open input */
  fd = open(fn, O_RDONLY | O_NOFOLLOW, 0);
  if (fd < 0) {
    dprintf(2, "Source \"%s\" open for reading failed\n", fn);
    fsync(2);
    exit(1);
  }

  /* Open output */
  fd2 = open(fn2, O_WRONLY | O_NOFOLLOW | O_CREAT | O_EXCL, 0666);
  if (fd2 < 0) {
    dprintf(2, "Destination \"%s\" open for writing failed\n", fn2);
    fsync(2);
    exit(1);
  }

  /* Write magic */
  e64(buf, DUMP_FILE_MAGIC);
  if (w(fd2, buf, 8) != 8) {
    dprintf(2, "Write error\n");
    fsync(2);
    exit(1);
  }

  /* Main loop */
  block = nblocks;
  do {
    uint64_t off, len;
    block--;
    off = block * FBLOCK_LEN;
    len = (off + FBLOCK_LEN) <= srcsize ? FBLOCK_LEN : srcsize - off;
    dprintf(1, "block=%lu, offset=%lu, length=%lu\n", (unsigned long)block, (unsigned long)off, (unsigned long)len);
    fsync(1);
    e64(buf, off);
    e64(buf + 8, len);
    if (w(fd2, buf, 16) != 16) {
      dprintf(2, "Write error\n");
      fsync(2);
      exit(1);
    }
    if (off != (off_t)off) {
      dprintf(2, "Bad offset\n");
      fsync(2);
      exit(1);
    }
    if (lseek(fd, (off_t)off, SEEK_SET) != (off_t)off) {
      dprintf(2, "Can't seek input\n");
      fsync(2);
      exit(1);
    }
    t0 = ts();
    while (len > 0) {
      size_t cl = (len <= FBUF_LEN) ? len : FBUF_LEN;
      if (r(fd, buf, cl) != cl) {
	dprintf(2, "Read error\n");
	fsync(2);
	exit(1);
      }
      if (w(fd2, buf, cl) != cl) {
	dprintf(2, "Write error\n");
	fsync(2);
	exit(1);
      }
      write(1, ".", 1);
      fsync(1);
      len -= cl;
      nbytes += cl;
    }
    t1 = ts();
    dprintf(1, "\nblock time %.6fs\n", t1 - t0);
    fsync(1);
  } while (block > 0);

  dprintf(1, "Input chunks written.\n");
  fsync(1);

  /* Write stumps. */
  if (stump != NULL) {
    dprintf(1, "Writing stumps.\n");
    fsync(1);
    for (tstump = stump; tstump != NULL; tstump = tstump->next) {
      t0 = ts();
      e64(buf, tstump->o);
      e64(buf + 8, tstump->l);
      if (w(fd2, buf, 16) != 16) {
	dprintf(2, "Write error\n");
	fsync(2);
	exit(1);
      }
      if (tstump->o != (off_t)tstump->o) {
	dprintf(2, "Bad offset\n");
	fsync(2);
	exit(1);
      }
      if (w(fd2, tstump->s, tstump->l) != tstump->l) {
	dprintf(2, "Write error\n");
	fsync(2);
	exit(1);
      }
      t1 = ts();
      dprintf(1, "stump time %.6fs\n", t1 - t0);
      fsync(1);
    }
    dprintf(1, "End of stumps reached.\n");
    fsync(1);
  }

  /* End marker */
  e64(buf, 0);
  e64(buf + 8, 0);
  if (w(fd2, buf, 16) != 16) {
    dprintf(2, "Write error\n");
    fsync(2);
    exit(1);
  }

  /* Close files */
  if (close(fd) != 0) {
    dprintf(2, "Closing source \"%s\" failed\n", fn);
    fsync(2);
    exit(1);
  }
  if (close(fd2) != 0) {
    dprintf(2, "Closing destination \"%s\" failed\n", fn2);
    fsync(2);
    exit(1);
  }

  /* Cleanup */
  free(buf);
  buf = NULL;
  while (stump != NULL) {
    tstump = stump;
    stump = tstump->next;
    free(tstump->s);
    free(tstump);
  }

  /* Final report */
  dprintf(1, "Written %lu blocks, %lu bytes total.\n", (unsigned long)nblocks, (unsigned long)nbytes);
  fsync(1);

  exit(0);
}
