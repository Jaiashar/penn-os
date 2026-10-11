CC = clang

# Replace -O3 with -g for a debug version during development
#CFLAGS = -Wall -Werror -O3
CFLAGS = -Wall -Werror -g

# Comment out the line below during development if assert(3) is used
CPPFLAGS = -DNDEBUG

SRCS=p_errno.c file_handle.c pennfat_fs.c pennfat_dir.c pennfat_io.c standalone_pennfat.c
OBJS = $(SRCS:.c=.o)

.PHONY : all clean

all : $(OBJS) pennfat

pennfat : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o pennfat

clean :
	$(RM) $(OBJS) pennfat
