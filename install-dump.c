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

#include "common.h"

const char *av0 = NULL;

int main(int argc, char **argv) {
  int restart = 0;
  const char *fn = NULL;
  struct stat st;
  int fd;
  char *buf = NULL;
  uint64_t nblocks = 0, nbytes = 0;
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
  while ((c = getopt(argc, argv, "+rS:")) > 0) {
      switch (c) {
      case 'r':
	restart = 1;
	break;
      case 'S':
	tstump = dump_stump_parse(optarg);
	if (tstump == NULL) {
	  dprintf(2, "%s: Invalid stump spec in option '%c'. Must be <offset>:<string>\n", av0, optopt);
	  dprintf(2, "Usage: %s [ -r ] <destination device or file>\n", av0);
	  fsync(2);
	  exit(1);
	}
	tstump->next = stump;
	stump = tstump;
	tstump = NULL;
	break;
      default:
	dprintf(2, "%s: Invalid option '%c'.\n", av0, optopt);
	dprintf(2, "Usage: %s [ -r ] <destination device or file>\n", av0);
	fsync(2);
	exit(1);
      }
  }
  argc -= optind;
  argv += optind;

  if (argc == 1) {
    fn = argv[0];
  } else {
    dprintf(2, "Usage: %s [ -r ] <destination device or file>\n", av0);
    fsync(2);
    exit(1);
  }

  if (lstat(fn, &st) != 0) {
    dprintf(2, "Can't stat destination \"%s\"\n", fn);
    fsync(2);
    exit(1);
  }

  /* Output type check */
  if (S_ISREG(st.st_mode)) {
    dprintf(1, "Destination is a regular file.\n");
    fsync(1);
  } else if (S_ISCHR(st.st_mode)) {
    dprintf(1, "Destination is a character device.\n");
    fsync(1);
  } else if (S_ISBLK(st.st_mode)) {
    dprintf(1, "Destination is a block device.\n");
    fsync(1);
  } else {
    dprintf(2, "Destination \"%s\" is not regular file or device node\n", fn);
    fsync(2);
    exit(1);
  }

  /* Buffer */
  if (! (buf = malloc(FBUF_LEN))) {
    dprintf(2, "Out of memory\n");
    fsync(2);
    exit(1);
  }

  /* Read and check magic */
  if (r(0, buf, 8) != 8) {
    dprintf(2, "Read error (magic)\n");
    fsync(2);
    exit(1);
  } 
  if (d64(buf) != DUMP_FILE_MAGIC) {
    dprintf(2, "Bad magic\n");
    fsync(2);
    exit(1);
  }

  /* Open output */
  fd = open(fn, O_WRONLY | /*O_SYNC |*/ O_NOFOLLOW, 0);
  if (fd < 0) {
    dprintf(2, "Destination \"%s\" open for writing failed\n", fn);
    fsync(2);
    exit(1);
  }

  /* Main loop */
  while (1) {
    uint64_t off, len;
    if (r(0, buf, 16) != 16) {
      dprintf(2, "Read error (header)\n");
      fsync(2);
      exit(1);
    } 
    off = d64(buf);
    len = d64(buf + 8);
    if ((off == 0) && (len == 0)) {
      dprintf(1, "End of input reached.\n");
      fsync(1);
      break;
    }
    nblocks++;
    dprintf(1, "block=%lu, offset=%lu, length=%lu\n", (unsigned long)nblocks, (unsigned long)off, (unsigned long)len);
    fsync(1);
    if (off != (off_t)off) {
      dprintf(2, "Bad offset\n");
      fsync(2);
      exit(1);
    }
    if (lseek(fd, (off_t)off, SEEK_SET) != (off_t)off) {
      dprintf(2, "Can't seek output\n");
      fsync(2);
      exit(1);
    }
    t0 = ts();
    while (len > 0) {
      size_t cl = (len <= FBUF_LEN) ? len : FBUF_LEN;
      if (r(0, buf, cl) != cl) {
	dprintf(2, "Read error (content)\n");
	fsync(2);
	exit(1);
      }
      if (w(fd, buf, cl) != cl) {
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
    dprintf(1, "\nblock=%lu ok in %.6fs\n", nblocks, t1 - t0);
    fsync(1);
  }

  /* Write stumps. */
  if (stump != NULL) {
    for (tstump = stump; tstump != NULL; tstump = tstump->next) {
      t0 = ts();
      if (lseek(fd, (off_t)tstump->o, SEEK_SET) != (off_t)tstump->o) {
	dprintf(2, "Can't seek output\n");
	fsync(2);
	exit(1);
      }
      if (w(fd, tstump->s, tstump->l) != tstump->l) {
	dprintf(2, "Write error\n");
	fsync(2);
	exit(1);
      }
      t1 = ts();
      dprintf(1, "stump ok in %.6fs\n", t1 - t0);
      fsync(1);
    }
    if (stump != NULL) {
      dprintf(1, "End of stumps reached.\n");
      fsync(1);
    }
  }

  /* Sync output */
  if (fsync(fd) != 0) {
    dprintf(2, "Destination sync failed\n");
    fsync(2);
    exit(1);
  }

  /* Close output */
  if (close(fd) != 0) {
    dprintf(2, "Closing destination \"%s\" failed\n", fn);
    fsync(2);
    exit(1);
  }

  /* Final report */
  dprintf(1, "Written %lu blocks, %lu bytes total.\n", (unsigned long)nblocks, (unsigned long)nbytes);
  fsync(1);

  /* Cleanup */
  free(buf);
  buf = NULL;
  while (stump != NULL) {
    tstump = stump;
    stump = tstump->next;
    free(tstump->s);
    free(tstump);
  }

  /* Immediate reboot */
  if (restart) {
    dprintf(1, "Attempting to reboot\n");
    fsync(1);
    sleep(1);
    REBOOT_NOW();
    sleep(1);
    dprintf(2, "Reboot failed errno=%d (%s)\n", errno, strerror(errno));
    fsync(2);
    exit(1);
  }
  exit(0);
}
