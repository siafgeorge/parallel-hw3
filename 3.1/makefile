Include = include
CC = mpicc
CFLAGS = -g -Wall -I$(Include)

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,bin/%.o,$(SRCS))

TARGET = polyno
Serial = -n 10 -s 1
Parallel = -n 1000000 -t 12

all: clean bin/$(TARGET)

bin/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

bin/%.o: src/%.c
	@mkdir -p bin
	$(CC) $(CFLAGS) -c $< -o $@

run: bin/$(TARGET)
	mpiexec	-n 4 --allow-run-as-root ./bin/$(TARGET) $(Serial)

# par: bin/$(TARGET)
# 	./bin/$(TARGET) $(Parallel)
# # 	python3 src/test.py

# valgrind: bin/$(TARGET)
# 	valgrind ./bin/$(TARGET) $(Serial)

clean:
	rm -rf bin