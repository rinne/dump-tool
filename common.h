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

#define FBUF_LEN 1048576
#define FBLOCK_LEN (FBUF_LEN * 128)

#define MAX_READ_SIZE 65536
#define MAX_WRITE_SIZE 65536

#define DUMP_FILE_MAGIC 0xfd54522d64756d70ull

typedef struct dump_stump_rec dump_stump_struct;
typedef struct dump_stump_rec *dump_stump_t;

struct dump_stump_rec {
  dump_stump_t next;
  uint64_t o;
  uint64_t l;
  char *s;
};

ssize_t r(int fd, void *buf, size_t count);
ssize_t w(int fd, const void *buf, size_t count);
uint64_t d64(const void *buf);
void e64(void *buf, uint64_t x);
double ts(void);
dump_stump_t dump_stump_parse(const char *x);
void REBOOT_NOW(void);
