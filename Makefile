SRCS = main.c sim_bp.c
INC = sim_bp.h
OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall -Werror -I.
LDFLAGS = -L.
# EXTRA_CFLAGS = -g
OPT_CFLAGS = -O3
DEBUG=0

CC = gcc

ifeq ($(DEBUG),1)
EXTRA_CFLAGS += -DDEBUG_FLAG
endif

ifeq ($(DEBUG_OP), 1)
EXTRA_CFLAGS += -DDEBUG_OP
endif

all: sim

sim: mycompile mylink

mycompile: $(OBJS) $(INC)

%.o: %.c $(INC)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(OPT_CFLAGS) -o $@ -c $<

mylink: $(OBJS) $(INC)
	$(CC) -o sim $(CFLAGS) $(EXTRA_CFLAGS) $(OBJS) -lm

clean:
	rm -f *.o sim

clobber:
	rm -f *.o



