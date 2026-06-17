CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
TARGET  = broolywood
SRCS    = main.c arvoreBmais.c disco.c hash_relacionamento.c loader.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf dados/

.PHONY: all clean
