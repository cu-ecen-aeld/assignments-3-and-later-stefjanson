CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Wextra

SRC_FILES = writer.c
OBJ_FILES = $(SRC_FILES:.c=.o)
TARGET = writer

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ_FILES)
