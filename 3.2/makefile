Include = include
CC = mpicc
CFLAGS = -g -Wall -I$(Include)

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,bin/%.o,$(SRCS))

Flags = -s 1000 -z 40 -m 15
TARGET = main

all: clean bin/$(TARGET)

bin/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

bin/%.o: src/%.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -c $< -o $@

run: bin/$(TARGET)
	mpiexec	-n 4 --allow-run-as-root ./bin/$(TARGET) $(Flags)

# par: bin/$(TARGET)
# 	./bin/$(TARGET) $(Parallel)
# # 	python3 src/test.py

# valgrind: bin/$(TARGET)
# 	valgrind ./bin/$(TARGET) $(Serial)

clean:
	rm -rf bin