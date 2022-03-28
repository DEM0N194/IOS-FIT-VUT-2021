CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic
LDFLAGS = -pthread -lrt
SRCS = proj2.c
TARGET = proj2

.PHONY: all $(TARGET) clean zip

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS) -o $(TARGET)

clean:
	rm $(TARGET) proj2.out

zip:
	zip $(TARGET).zip Makefile $(SRCS)
