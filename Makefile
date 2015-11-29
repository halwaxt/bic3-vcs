CC          = gcc
CFLAGS      = -Wall -Werror -Wextra -Wstrict-prototypes -pedantic -fno-common -g -O3 -std=gnu11
DOXYGEN     = doxygen
SRCDIR      = src
OBJDIR      = obj
BINDIR      = bin
CP          = cp
CD          = cd
MV          = mv
GREP        = grep
CLIENT_DIR  = ./simple_message_client
SERVER_DIR  = ./simple_message_server

default: all

all: client_server

.PHONY: client_server
client_server:
	$(MAKE) -C $(CLIENT_DIR)
	$(MAKE) -C $(SERVER_DIR)