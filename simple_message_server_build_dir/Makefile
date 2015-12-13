CC=gcc52
CFLAGS=-DDEBUG -Wall -Werror -Wextra -Wstrict-prototypes -pedantic -fno-common -g -O3 -std=gnu11
CP=cp
CD=cd
MV=mv
RM=rm

OBJECTS=simple_message_server.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

simple_message_server: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^
