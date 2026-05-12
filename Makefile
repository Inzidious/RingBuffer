flags=-O2 -Wall -g
ldflags=-lbu

all: clean ringbuff

ringbuff: ringbuffer.o
	cc $(flags) $^ -o $@

ringbuff.o: ringbuffer.c ringbuffer.h
	cc $(flags) -c $<

clean:
	rm -f *.o ringbuff