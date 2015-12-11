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
  const char *fn = NULL;
  struct stat st;
  int fd, i;
  char *buf = NULL;
  uint64_t srcsize = 0, len = 64, off = DUMP_INVALID_OFFSET;
  const char *hlp;
  int c;

  (void)ts();

  /* Arguments */
  hlp = strrchr(argv[0], '/');
  if (hlp == NULL)
    av0 = argv[0];
  else
    av0 = hlp + 1;
  opterr = 0;
  while ((c = getopt(argc, argv, "+o:l:")) > 0) {
      switch (c) {
      case 'o':
	off = offset_parse(optarg);
	if (off == DUMP_INVALID_OFFSET) {
	  dprintf(2, "%s: Bad offset in '%c'.\n", av0, optopt);
	  fsync(2);
	  exit(1);
	}
	break;
      case 'l':
	len = offset_parse(optarg);
	if ((len == DUMP_INVALID_OFFSET) || (len > 0x10000)) {
	  dprintf(2, "%s: Bad length in '%c'.\n", av0, optopt);
	  fsync(2);
	  exit(1);
	}
	break;
      default:
	dprintf(2, "%s: Invalid option '%c'.\n", av0, optopt);
	dprintf(2, "Usage: %s -o <offset> [ -l <maximum len> ] <source device or file>\n", av0);
	fsync(2);
	exit(1);
      }
  }
  if ((off + len) < off) {
    dprintf(2, "%s: Bad offset in '%c'.\n", av0, optopt);
    fsync(2);
    exit(1);
  }
  argc -= optind;
  argv += optind;
  if (argc == 1) {
    fn = argv[0];
  } else {
    dprintf(2, "Usage: %s -o <offset> [ -l <maximum len> ] <source device or file>\n", av0);
    fsync(2);
    exit(1);
  }
  if (lstat(fn, &st) != 0) {
    dprintf(2, "Can't stat source \"%s\"\n", fn);
    fsync(2);
    exit(1);
  }

  /* Input type check and check */
  if (S_ISREG(st.st_mode)) {
    srcsize = st.st_size;
  } else if (S_ISCHR(st.st_mode)) {
    srcsize = st.st_size;
  } else if (S_ISBLK(st.st_mode)) {
    if (((fd = open(fn, O_RDONLY | O_NOFOLLOW, 0)) < 0) || (ioctl(fd, BLKGETSIZE64, &srcsize) != 0) || (close(fd) != 0)) {
      dprintf(2, "Can't get size of the block device \"%s\"\n", fn);
      fsync(2);
      exit(1);
    }
  } else {
    dprintf(2, "Input \"%s\" is not regular file or device node\n", fn);
    fsync(2);
    exit(1);
  }
  if (off >= srcsize) {
    dprintf(2, "Offset (%lu) is beyond the size of the source file (%lu).\n", (unsigned long)off, (unsigned long)srcsize);
    fsync(2);
    exit(1);
  }
  if ((off + len) > srcsize) {
    len = srcsize - off - 1;
  }

  /* Buffer */
  if (! (buf = malloc(len + 1))) {
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

  /* Seek and read */
  if (lseek(fd, (off_t)off, SEEK_SET) != (off_t)off) {
    dprintf(2, "Can't seek input\n");
    fsync(2);
    exit(1);
  }
  if (r(fd, buf, len + 1) != (len + 1)) {
    dprintf(2, "Read error\n");
    fsync(2);
    exit(1);
  }

  /* Close files */
  if (close(fd) != 0) {
    dprintf(2, "Closing source \"%s\" failed\n", fn);
    fsync(2);
    exit(1);
  }

  for (i = 0; i <= len; i++) {
    if (buf[i] == '\0') {
      break;
    }
  }
  if (i > len) {
    dprintf(2, "Peeked data is not terminated.\n");
    fsync(2);
    exit(1);
  }

  /* We got it and let's just print it. */
  dprintf(1, "%s\n", buf);
  fsync(1);

  /* Cleanup */
  free(buf);
  buf = NULL;
  exit(0);
}
