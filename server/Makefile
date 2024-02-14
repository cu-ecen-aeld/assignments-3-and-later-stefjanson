CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra

SRC_FILES = aesdsocket.c
OBJ_FILES = $(SRC_FILES:.c=.o)
TARGET = aesdsocket

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ_FILES)
