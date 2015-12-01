//
//
//  simple_message_client
//
//  Created by Thomas {Halwax,Zeitinger} on 24.11.15.
//  Copyright © 2015 Technikum Wien. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "simple_message_client_commandline_handling.h"

#define BUF_SIZE 6000 /* Examples in protocol analysis were about 4500 bytes */

void showUsage(FILE *stream, const char *cmnd, int exitcode);
ssize_t write_sock(int fd, const void *puffer, size_t n);
ssize_t read_sock(int fd, void *puffer, size_t n);

int main(int argc, const char * argv[]) {
    /*
     simple_message_client  -s server -p port -u user [-i image URL] -m mes-
     sage [-v] [-h]
     */
    const char *server;
    const char *port;
    const char *user;
    const char *image_url;
    const char *message;
    int verbose;

    smc_parsecommandline(argc, argv, showUsage, &server, &port, &user, &message, &image_url, &verbose);
    printf("All params parsed correctly\n");

    /* ToDos:
     * Create connection to server
     * Send message
     * wait for reply
     *
     * see man getaddrinfo
     */

    struct addrinfo hints;
    struct addrinfo *result, *rp;
//    int sfd, s, j;
    int sfd, s;
//    size_t len;
    ssize_t nread;
    char buf[BUF_SIZE];

    /* Obtain address(es) matching host/port */

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Stream socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(server, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */

    /* Send remaining command-line arguments as separate
       datagrams, and read responses from server */

	/* Build complete message in buffer, reuse buffer later for receiving */

	strncpy(buf, "user=", 5);
    if ((strlen(buf) + 1) > BUF_SIZE) {
        fprintf(stderr, "Message to long");
        exit(EXIT_FAILURE);
    }
	strncat(buf, user, strlen(user));
    if ((strlen(buf) + 1) > BUF_SIZE) {
        fprintf(stderr, "Message to long");
        exit(EXIT_FAILURE);
    }
	strncat(buf, "\n", 1);
    if ((strlen(buf) + 1) > BUF_SIZE) {
        fprintf(stderr, "Message to long");
        exit(EXIT_FAILURE);
    }
	if (image_url != NULL) {
		strncat(buf, "img=", 4);
    	if ((strlen(buf) + 1) > BUF_SIZE) {
        	fprintf(stderr, "Message to long");
        	exit(EXIT_FAILURE);
    	}
		strncat(buf, image_url, strlen(image_url));
    	if ((strlen(buf) + 1) > BUF_SIZE) {
        	fprintf(stderr, "Message to long");
        	exit(EXIT_FAILURE);
    	}
		strncat(buf, "\n", 1);
    	if ((strlen(buf) + 1) > BUF_SIZE) {
        	fprintf(stderr, "Message to long");
        	exit(EXIT_FAILURE);
    	}
	}
	strncat(buf, message, strlen(message));
    if ((strlen(buf) + 1) > BUF_SIZE) {
        fprintf(stderr, "Message to long");
        exit(EXIT_FAILURE);
    }

	printf("Message: %s", buf);
    if ((write_sock(sfd, &buf, sizeof(buf))) != sizeof(buf)) {
        fprintf(stderr, "partial/failed write\n");
        exit(EXIT_FAILURE);
    }

	/* clean buf */
	memset(&buf, 0, BUF_SIZE);
	/* Close connection */
	if (shutdown(sfd, SHUT_RD) == -1) {
		perror("shutdown");
		exit(EXIT_FAILURE);
	}

    nread = read_sock(sfd, buf, BUF_SIZE);
    if (nread == -1) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    printf("Received %zd bytes: %s\n", nread, buf);

	/* Close sfd */
	if (close(sfd) == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}

    exit(EXIT_SUCCESS);
}


void showUsage(FILE *stream, const char *cmnd, int exitcode) {
    fprintf(stream, "%s: %s\n\n", cmnd, "invalid params");
    fprintf(stream, "%s: %s\n", cmnd, "-s server -p port -u user [-i image URL] -m message [-v] [-h]");
    exit(exitcode);
}

/* copy & paste aus dem buch kapitel */
ssize_t write_sock(int fd, const void *puffer, size_t n) {
	size_t      verbleiben = n;
	ssize_t     geschrieben;
	const char *zgr = puffer;
	while (verbleiben > 0) { /* schreibt n Bytes nach fd */
		if ( (geschrieben = write(fd, zgr, verbleiben)) <= 0) {
			if (errno == EINTR)
				geschrieben = 0;  /* neuer write-Versuch */
			else
				fprintf(stderr, "write_sock-Fehler");
		}
		verbleiben -= geschrieben;
		zgr        += geschrieben;
	}
	return n;
}
ssize_t read_sock(int fd, void *puffer, size_t n) {
	size_t   verbleiben = n;
	ssize_t  gelesen;
	char    *zgr = puffer;
	while (verbleiben > 0) { /* liest n Bytes von fd */
		if ( (gelesen = read(fd, zgr, verbleiben)) < 0) {
			if (errno == EINTR)
				gelesen = 0;  /* neuer read-Versuch */
			else
				fprintf(stderr, "read_sock-Fehler");
		} else if (gelesen == 0)
			break; /* EOF */
			verbleiben -= gelesen;
			zgr        += gelesen;
	}
	return n-verbleiben;
}
