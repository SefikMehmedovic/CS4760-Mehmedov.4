#makefile for P3
#
CLFAGS = -g -Wall -std=c99

objects = oss user
all: $(objects)

$(objects): %: %.c
	$(CC) $(CFLAGS) -o $@ $< -pthread

clean:
	rm $(objects)

