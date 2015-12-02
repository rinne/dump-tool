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

ssize_t r(int fd, void *buf, size_t count);
ssize_t w(int fd, const void *buf, size_t count);
uint64_t d64(const void *buf);
void e64(void *buf, uint64_t x);
double ts(void);
void REBOOT_NOW(void);
