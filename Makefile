OBJS =  focusimage.o ahd_bayer.o
CC = gcc
CFLAGS = -g -Wall 
LDFLAGS =  -lnetpbm

.PHONY: all
all: refocus

refocus: refocus.o $(OBJS)
	$(CC) -o $@ refocus.o $(OBJS) $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) refocus.o refocus
