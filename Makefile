#
#  Dump Tool
#
#  See README.md
#
#  Copyright (C) 2015 Timo J. Rinne <tri@iki.fi>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.
#

CFLAGS += -O6 -Wall -g
LDFLAGS += -static

all: create-dump install-dump peek-string

clean:
	rm -f create-dump install-dump peek-string *.o

create-dump: create-dump.o common.o

install-dump: install-dump.o common.o

peek-string: peek-string.o common.o
