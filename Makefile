PHONY := all

LDFLAGS := -lpthread
CC := clang++

test : main.cpp
	$(CC) $(LDFLAGS) -o $@  $?

all:test

