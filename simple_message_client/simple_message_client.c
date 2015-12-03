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

#define ERROR -1
#define SUCCESS 0
#define BUF_SIZE 6000 /* Examples in protocol analysis were about 4500 bytes */

void showUsage(FILE *stream, const char *cmnd, int exitcode);


int sendData(FILE *target, const char *key, const char *payload);
int checkServerResponseStatus(FILE *source);
int writeOutputToFile(FILE *source);
int getOutputFileLength(FILE *source, int *value);
int getOutputFileName(FILE *source, char *value);

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
    int sfd = -1;
    int s = -1;
//    size_t len;
    //ssize_t nread;
    //char buf[BUF_SIZE];

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

    FILE *target = fdopen(sfd, "w");
    
    sendData(target, "user=", user);
    sendData(target, "", message);
    
    /* fclose schließt auch sfd, daher vorher ein DUP2 */
    
    int backupOfSfd = dup(sfd);
    
    shutdown(sfd, SHUT_RD);
    /* destroys also sfd */
    fclose(target);
    
    FILE *fromServer = fdopen(backupOfSfd, "r");
    
    /* read line for status=... */
    /* if status returned from server != 0 then exit using the status */
    int status = 0;
    if ((status = checkServerResponseStatus(fromServer)) != 0) {
        /* TODO: cleanup */
        exit(status);
    }
    
    int done = 0;
    do {
        done = writeOutputToFile(fromServer);
    } while (! done);
    
    
    fclose(fromServer);
    close(backupOfSfd);
}


int sendData(FILE *target, const char *key, const char *payload) {
    fprintf(target, "%s", key);
    fprintf(target, "%s", payload);
    fprintf(target, "\n");
    fflush(target);
    
    return 1;
}

int checkServerResponseStatus(FILE *source) {
    /* read line from source */
    /* compare n chars with status=0 */
    char *line = NULL;
    char *key = NULL;
    size_t sizeOfLine = 0;
    int value = 0;

    getline(&line, &sizeOfLine, source);
    
    sscanf(line, "%s=%d", key, &value);
    free(line);
    
    return value;
}

int getOutputFileName(FILE *source, char *value) {
    char *line = NULL;
    size_t sizeOfLine = 0;
    
    errno = SUCCESS;
    if (getline(&line, &sizeOfLine, source) < SUCCESS) {
        if (errno == EINVAL || errno == EOVERFLOW) {
            free(line);
            return ERROR;
        }
    }
    
    if (sscanf(line, "file=%s", value) <= SUCCESS) {
        free(line);
        return ERROR;
    }
    
    return SUCCESS;
}

int getOutputFileLength(FILE *source, int *value) {
    char *line = NULL;
    size_t sizeOfLine = 0;
    
    errno = SUCCESS;
    if (getline(&line, &sizeOfLine, source) < SUCCESS) {
        if (errno == EINVAL || errno == EOVERFLOW) {
            free(line);
            return ERROR;
        }
    }
    
    if (sscanf(line, "len=%d", value) <= SUCCESS) {
        free(line);
        return ERROR;
    }
    
    return SUCCESS;
}



int writeOutputToFile(FILE *source) {
    char *fileName = NULL;
    int length = 0;
    
    if (getOutputFileName(source, fileName) != SUCCESS) return ERROR;
    if (getOutputFileLength(source, &length) != SUCCESS) return ERROR;
    
    printf("i'd write %d bytes to %s\n", length, fileName);
    
    
    /* check for EOF */
    /* read line file=.... */
    /* read line len=.... */
    /* check if bytes read < or > len */
    /* read bytes until remaining len = 0 */
    
    return 0;
}


void showUsage(FILE *stream, const char *cmnd, int exitcode) {
    fprintf(stream, "%s: %s\n\n", cmnd, "invalid params");
    fprintf(stream, "%s: %s\n", cmnd, "-s server -p port -u user [-i image URL] -m message [-v] [-h]");
    exit(exitcode);
}

