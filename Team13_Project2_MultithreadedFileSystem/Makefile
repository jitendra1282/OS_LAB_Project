CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -pthread

SRCS = thread_pool.c logger.c rwlock_manager.c main.c
OBJS = $(SRCS:.c=.o)
TARGET = fs_sim

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) ops.log

.PHONY: all clean
