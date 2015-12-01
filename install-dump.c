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
  int restart = 0;
  const char *fn = NULL;
  struct stat st;
  int fd;
  char *buf = NULL;
  uint64_t nblocks = 0, nbytes = 0;

  (void)ts();

  /* Arguments */
  if ((argc == 3) && (strcmp(argv[1], "-r") == 0) && (*(argv[2]) != '-')) {
    restart = 1;
    fn = argv[2];
  } else if ((argc == 2) && (*(argv[1]) != '-')) {
    restart = 0;
    fn = argv[1];
  } else {
    dprintf(2, "Usage: install-dump [ -r ] <destination device or file>\n");
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
    double t0, t1;
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