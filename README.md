This ringbuffer is a single producer, multiple consumer circular queue that uses atomics to maintain lock free read and write functionality.

There's a mutex based write function which the profiler uses to compare write speeds against the atomic version.

Makefile uses cc to compile into a ./ringbuff executable for execution. 
