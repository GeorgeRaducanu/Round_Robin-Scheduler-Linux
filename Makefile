CC = gcc
CFLAGS = -fpic -Wall -Wextra

.PHONY: build

build: libscheduler.so

libscheduler.so : so_scheduler.o
	$(CC) -shared -o $@ $^

so_scheduler.o: so_scheduler.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -f *.o libscheduler.so

