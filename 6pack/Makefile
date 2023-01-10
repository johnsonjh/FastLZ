# SPDX-License-Identifier: MIT
# Copyright (c) 2005-2020 Ariya Hidayat <ariya.hidayat@gmail.com>
# Copyright (c) 2023 Jeffrey H. Johnson <trnsz@pobox.com>

CFLAGS ?= -Wall -std=c90 -Wextra -Wpedantic -Ofast -flto=auto -Wno-declaration-after-statement

all: 6pack 6unpack

6pack: 6pack.c ../fastlz.c
	$(CC) -o 6pack $(CFLAGS) -I.. 6pack.c ../fastlz.c

6unpack: 6unpack.c ../fastlz.c
	$(CC) -o 6unpack $(CFLAGS) -I.. 6unpack.c ../fastlz.c

clean :
	$(RM) 6pack 6unpack *.o