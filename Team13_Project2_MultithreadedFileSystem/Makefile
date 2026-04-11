CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -g -O2
LDFLAGS = -pthread

# All source files in the project
SRCS = thread_pool.c \
       logger.c \
       rwlock_manager.c \
       file_rw.c \
       file_ops.c \
       main.c

OBJS   = $(SRCS:.c=.o)
TARGET = fs_sim

.PHONY: all clean run

# Default: build the binary
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete → ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the demo
run: all
	@rm -f ops.log
	./$(TARGET)
	@echo ""
	@echo "─── ops.log ─────────────────────────────────────────"
	@cat ops.log

# Clean build artifacts and generated files
clean:
	rm -f $(OBJS) $(TARGET) ops.log test_shared.txt test_renamed.txt *.o
