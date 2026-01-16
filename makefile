Include = include
CC = gcc
CFLAGS = -g3 -pedantic -I $(Include) -Wall -Werror -fopenmp

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,bin/%.o,$(SRCS))

TARGET = polyno
Serial = -s 10 -z 100 -m 1 -t 1 -d
Parallel = -s 1000 -z 30 -m 5 -t 12 -d

all: bin/$(TARGET)

bin/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

bin/%.o: src/%.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -c $< -o $@

run: bin/$(TARGET)
	./bin/$(TARGET) $(Serial)

clean:
	rm -rf bin