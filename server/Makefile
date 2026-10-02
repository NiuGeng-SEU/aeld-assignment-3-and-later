CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g -O2
TARGET ?= aesdsocket
SRCS ?= aesdsocket.c
OBJS ?= $(SRCS:.c=.o)

.PHONY: all default clean

default: $(TARGET)
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
