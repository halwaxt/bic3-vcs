CC=gcc52
CFLAGS=-DDEBUG -Wall -Werror -Wextra -Wstrict-prototypes -pedantic -fno-common -g -O3 -std=gnu11
CP=cp
CD=cd
MV=mv
RM=rm

OBJECTS=simple_message_client.o simple_message_client_commandline_handling.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

simple_message_client: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^
