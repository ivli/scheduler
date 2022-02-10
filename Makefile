
LDFLAGS := -lpthread
CC := clang++

test : $(wildcard *.cpp) 
	$(CC) $(LDFLAGS) -o $@  $?

all : test