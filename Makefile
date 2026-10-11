PROG=penn-os

PROMPT='"$$ "'

CC = clang
CFLAGS = -Wall -Werror -O3 -DPROMPT=$(PROMPT)
CPPFLAGS = -DNDEBUG

PROG = bin/penn-os
PENNFATPROG = bin/pennfat

# replace this with shell when implemented
MAIN = src/penn_shell.c
SRCS := $(wildcard src/kernel/*.c) $(wildcard src/os/*.c) $(wildcard src/pennfat/*.c) $(wildcard src/shell/*.c)
SRCS := $(filter-out src/pennfat/standalone_pennfat.c, $(SRCS))
SRCS := $(filter-out src/pennfat/p_errno.c, $(SRCS))
OBJS :=  $(SRCS:.c=.o) src/shell/parsejob.o

PENNFATSRCS = $(wildcard src/pennfat/*.c)
PENNFATOBJS = $(PENNFATSRCS:.c=.o)

.PHONY : clean all

all: $(PROG) $(PENNFATPROG)

$(PROG) : $(OBJS) $(MAIN)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(MAIN) $(OBJS)

$(PENNFATPROG) : $(PENNFATOBJS)
	mkdir -p bin
	$(CC) $(CFLAGS) $(PENNFATOBJS) -o $(PENNFATPROG)

SRCDEL = $(wildcard src/kernel/*.c) $(wildcard src/os/*.c) $(wildcard src/pennfat/*.c) $(wildcard src/shell/*.c)
OBJDEL = $(SRCDEL:.c=.o)

clean :
	$(RM) $(PROG) $(OBJDEL) $(PENNFATPROG)
