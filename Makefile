CC = gcc
CFLAGS = -Wall -g -pthread
TARGET = proxy_server
SRC = src/proxy.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)