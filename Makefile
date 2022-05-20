# Makefile template for small simple projects
SRCS = $(wildcard *.c)
EXEC = timestamp

CC = gcc
CFLAGS = -c -g -Wall -Wextra -Wpedantic
# to pass an include path to the compiler, add: -I./
#LDIR = -L./
LFLAGS = -flto -no-pie
# to pass a binary path to the linker: -Wl,-rpath,./
OBJS = $(SRCS:.c=.o)

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDIR) $(LFLAGS)

%.o:%.c %.h
	$(CC) $(CFLAGS) $< -o $@

# $@ is the left side of the :
# $^ is the right side of the :
# the $< is the first item in the dependencies list
# $@: $< $^

.PHONY: clean oclean release

clean: oclean
	rm -f $(EXEC)

oclean:
	rm -f $(OBJS)

release: all oclean

