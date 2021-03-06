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
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define USE_GLIB_REBOOT 1

#ifdef USE_GLIB_REBOOT
# include <sys/reboot.h>
#else
# include <linux/reboot.h>
#endif

#include "common.h"

ssize_t r(int fd, void *buf, size_t count) {
  size_t off = 0;
  ssize_t rv;
  while (off < count) {
    rv = read(fd, buf + off, ((count - off) <= MAX_READ_SIZE) ? count - off : MAX_READ_SIZE);
    if (rv < 0) {
      return -1;
    } else if (rv == 0) {
      return (off == 0) ? 0 : -1;
    }
    off += rv;
  }
  return off;
}

ssize_t w(int fd, const void *buf, size_t count) {
  size_t off = 0;
  ssize_t rv;
  while (off < count) {
    rv = write(fd, buf + off, ((count - off) <= MAX_WRITE_SIZE) ? count - off : MAX_WRITE_SIZE);
    if (rv <= 0) {
      return -1;
    }
    off += rv;
  }
  return off;
}

uint64_t d64(const void *buf) {
  const unsigned char *cb = buf;
  uint64_t r = 0;
  int i;
  for (i = 0; i < 8; i++) {
    r = (r << 8) | cb[i];
  }
  return r;
}

void e64(void *buf, uint64_t x) {
  unsigned char *cb = buf;
  int i;
  for (i = 7; i >= 0; i--) {
    cb[i] = x & 0xff;
    x >>= 8;
  }
}

double ts() {
  static double t0 = -1.0, t1;
  struct timeval tv;
  time_t t;

  if (gettimeofday(&tv, NULL) != 0) {
    t = time(NULL);
    tv.tv_sec = t;
    tv.tv_usec = 0;
  }
  t1 = (((double)tv.tv_usec) / 1000000.0) + ((double)tv.tv_sec);
  if (t0 < 0.0) {
    t0 = t1;
  }
  return t1 - t0;
}


uint64_t offset_parse(const char *x) {
  char *hlp;
  uint64_t o;
  char buf[64];

  o = (uint64_t)strtoul(x, &hlp, 0);
  if (*hlp != '\0') {
    return DUMP_INVALID_OFFSET;
  }
  if (snprintf(buf, sizeof (buf), "%lu", (unsigned long)o) >= sizeof(buf)) {
    return DUMP_INVALID_OFFSET;
  }
  if (strcmp(buf, x) != 0) {
    return DUMP_INVALID_OFFSET;
  }
  if (o != ((off_t)o)) {
    return DUMP_INVALID_OFFSET;
  }
  return o;
}

dump_stump_t dump_stump_parse(const char *x) {
  char *hlp;
  uint64_t o;
  char *s = NULL;
  char buf[64];
  dump_stump_t r = NULL;

  hlp = strrchr(x, ':');
  if (hlp == NULL) {
    goto error;
  }
  if (*(hlp + 1) == '\0') {
    goto error;
  }
  s = strdup(hlp + 1);
  if (s == NULL) {
    goto error;
  }
  o = (uint64_t)strtoul(x, &hlp, 0);
  if (*hlp != ':') {
    goto error;
  }
  if (snprintf(buf, sizeof (buf), "%lu:", (unsigned long)o) >= sizeof(buf)) {
    goto error;
  }
  if (strncmp(buf, x, strlen(buf)) != 0) {
    goto error;
  }
  if (o != ((off_t)o)) {
    goto error;
  }
  r = calloc(1, sizeof (*r));
  r->s = s;
  r->l = ((uint64_t)(strlen(r->s))) + 1; /* Also trailing NUL is included in length. */
  r->o = o;
  return r;
 error:
  if (s != NULL) {
    free(s);
  }
  if (r != NULL) {
    if (r->s != NULL) {
      free(r->s);
    }
    free(r);
  }
  return NULL;
}

void REBOOT_NOW() {
#ifdef USE_GLIB_REBOOT
  reboot(RB_AUTOBOOT);
#else
  reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2A, LINUX_REBOOT_CMD_RESTART, NULL);
#endif
}
