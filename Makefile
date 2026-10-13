
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Iinclude

TARGET = boxforge
SOURCES = src/main.c src/cgroups.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean

