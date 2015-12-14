# vim: set ts=4 sw=4 sts=4 :
##
## @file Makefile
## TCP/IP Network Programming
## Simple Client/Server
##
## @author Thomas Halwax <ic14b050@technikum-wien.at>
## @author Thomas Zeitinger <ic14b033@technikum-wien.at>
## @date 2015/12/13
##
## @version $Revision: 1.0 $
##
## @todo everything is done
##
## URL: $HeadURL$
##
## Last Modified: $Author: thomas $
##

##
## ------------------------------------------------------------- variables --
##

CC=gcc52
CFLAGS=-DDEBUG -Wall -Werror -Wextra -Wstrict-prototypes -pedantic -fno-common -g -O3 -std=gnu11
CP=cp
CD=cd
MV=mv
RM=rm
EP=grep
DOXYGEN=doxygen

OBJECTS_SERVER=simple_message_server.o
OBJECTS_CLIENT=simple_message_client.o simple_message_client_commandline_handling.o

##
## ----------------------------------------------------------------- rules --
##

%.o : %.c
	$(CC) $(CFLAGS) -c $<

##
## --------------------------------------------------------------- targets --
##

all: simple_message_client simple_message_server

simple_message_server: $(OBJECTS_SERVER)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

simple_message_client: $(OBJECTS_CLIENT)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	$(RM) simple_message_client.o simple_message_client simple_message_server.o simple_message_server 

##
## ---------------------------------------------------------- dependencies --
##

##
## =================================================================== eof ==
##

