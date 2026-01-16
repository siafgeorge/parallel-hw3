Include = include
CC = mpicc
CFLAGS = -g -Wall -I$(Include)

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,bin/%.o,$(SRCS))

TARGET = main

all: clean bin/$(TARGET)

bin/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

bin/%.o: src/%.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -c $< -o $@

run: bin/$(TARGET)
	mpiexec	-n 4 ./bin/$(TARGET)

# par: bin/$(TARGET)
# 	./bin/$(TARGET) $(Parallel)
# # 	python3 src/test.py

# valgrind: bin/$(TARGET)
# 	valgrind ./bin/$(TARGET) $(Serial)

clean:
	rm -rf bin