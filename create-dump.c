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
#include <sys/types.h>
#include <sys/stat.h>

#include "common.h"

int main(int argc, char **argv) {
  const char *fn = NULL, *fn2 = NULL;
  struct stat st;
  int fd, fd2;
  char *buf = NULL;
  uint64_t srcsize = 0, nblocks = 0, block = 0, nbytes = 0;

  (void)ts();

  /* Arguments */
  if ((argc == 3) && (*(argv[1]) != '-') && (*(argv[2]) != '-')) {
    fn = argv[1];
    fn2 = argv[2];
  } else {
    dprintf(2, "Usage: create-dumper <source device or file> <destination-file>\n");
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
    dprintf(1, "Input is a regular file.\n");
    fsync(1);
  } else if (S_ISCHR(st.st_mode)) {
    dprintf(1, "Input is a character device.\n");
    fsync(1);
  } else if (S_ISBLK(st.st_mode)) {
    dprintf(1, "Input is a block device.\n");
    fsync(1);
  } else {
    dprintf(2, "Input \"%s\" is not regular file or device node\n", fn);
    fsync(2);
    exit(1);
  }

  /* Size calculation */
  srcsize = st.st_size;
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

  /* Main loop */
  block = nblocks;
  do {
    uint64_t off, len;
    double t0, t1;
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
  e64(buf, 0);
  e64(buf + 8, 0);
  if (w(fd2, buf, 16) != 16) {
    dprintf(2, "Write error\n");
    fsync(2);
    exit(1);
  }
  write(1, "eof\n", 4);
  fsync(1);

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

  /* Final report */
  dprintf(1, "Written %lu blocks, %lu bytes total.\n", (unsigned long)nblocks, (unsigned long)nbytes);
  fsync(1);

  exit(0);
}
