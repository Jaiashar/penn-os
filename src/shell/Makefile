CC = clang

# Replace -O3 with -g for a debug version during development
CFLAGS = -Wall -Werror -g

PROMPT='"$$ "'
# Remove -DNDEBUG during development if assert(3) is used
#
override CPPFLAGS += -DNDEBUG -DPROMPT=$(PROMPT)


SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

.PHONY : all clean

all : $(OBJS) $@ $^ parsejob.o

clean :
	$(RM) $(OBJS)
