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
#include <fcntl.h>
#include "simple_message_client_commandline_handling.h"

#define ERROR -1
#define SUCCESS 0
#define DONE 2

void showUsage(FILE *stream, const char *cmnd, int exitcode);

int connetToServer(const char *server, const char *port, int *socketDescriptor);
int sendData(FILE *target, const char *key, const char *payload);
int checkServerResponseStatus(FILE *source, int *status);
int transferFile(FILE *source);
int getOutputFileLength(FILE *source, unsigned long *value);
int getOutputFileName(FILE *source, char **value);

static const char *programName;

int main(int argc, const char * argv[]) {

    const char *server;
    const char *port;
    const char *user;
    const char *image_url;
    const char *message;
    int verbose;

    programName = argv[0];
    
    smc_parsecommandline(argc, argv, showUsage, &server, &port, &user, &message, &image_url, &verbose);

    int sfd = 0;
    if (connetToServer(server, port, &sfd) != SUCCESS) {
        exit(errno);
    }
    
    errno = SUCCESS;
    FILE *toServer = fdopen(sfd, "w");
    if (toServer == NULL) {
        perror(programName);
        close(sfd);
        exit(errno);
    }
    
    if (sendData(toServer, "user=", user) == ERROR) {
        perror(programName);
        shutdown(sfd, SHUT_RDWR);
        fclose(toServer);
        exit(errno);
    }
    
    if (image_url != NULL) {
        if (sendData(toServer, "img=", image_url) == ERROR) {
            perror(programName);
            shutdown(sfd, SHUT_RDWR);
            fclose(toServer);
            exit(errno);
        }
    }
    
    if (sendData(toServer, "", message) == ERROR) {
        perror(programName);
        shutdown(sfd, SHUT_RDWR);
        fclose(toServer);
        exit(errno);
    }
    
    /* fclose schließt auch sfd, daher vorher ein dup */
    
    int backupOfSfd = dup(sfd);
    
    shutdown(sfd, SHUT_WR);
    fclose(toServer);
    
    FILE *fromServer = fdopen(backupOfSfd, "r");
    if (fromServer == NULL) {
        perror(programName);
        close(backupOfSfd);
        exit(errno);
    }
    
    /* read line for status=... */
    /* if status returned from server != 0 then exit using the status */
    int status = ERROR;
    
    if (checkServerResponseStatus(fromServer, &status) != SUCCESS || status != SUCCESS) {
        fprintf(stderr, "%s: reading server response failed with error %d\n", programName, status);
        fclose(fromServer);
        close(backupOfSfd);
        exit(status);
    }
    
    int canTransferFile = SUCCESS;
    while (canTransferFile != DONE) {
        canTransferFile = transferFile(fromServer);
        if (canTransferFile == ERROR) {
            fclose(fromServer);
            close(backupOfSfd);
            exit(EXIT_FAILURE);
        }
    }
    
    fclose(fromServer);
    close(backupOfSfd);
    exit(EXIT_SUCCESS);
}


int connetToServer(const char *server, const char *port, int *socketDescriptor) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd = -1;
    
    /* Obtain address(es) matching host/port */
    
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Stream socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */
    

    if (getaddrinfo(server, port, &hints, &result) != SUCCESS) {
        fprintf(stderr, "%s: getaddrinfo: %s\n", programName, gai_strerror(errno));
        return ERROR;
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

    freeaddrinfo(result);           /* No longer needed */
    
    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "%s: Could not connect\n", programName);
        return ERROR;
    }
    
    socketDescriptor = &sfd;
    return SUCCESS;

}


int sendData(FILE *target, const char *key, const char *payload) {
    if (fprintf(target, "%s", key) < 0) return ERROR;
    if (fprintf(target, "%s", payload) < 0) return ERROR;
    if (fprintf(target, "\n") < 0) return ERROR;
    if (fflush(target) == EOF) return ERROR;
    
    return SUCCESS;
}

int checkServerResponseStatus(FILE *source, int *status) {
    /* read line from source */
    /* compare n chars with status=0 */
    char *line = NULL;
    size_t sizeOfLine = 0;
    int found = 0;
    

    errno = SUCCESS;
    if (getline(&line, &sizeOfLine, source) < SUCCESS) {
        free(line);
        if (errno == EINVAL || errno == EOVERFLOW) {
            fprintf(stderr, "getline failed\n");
            return ERROR;
        }
        else {
            /* must be EOF */
            return DONE;
        }
    }

    found = sscanf(line, "status=%d", status);
    if (found == 0 || found == EOF) {
        fprintf(stderr, "pattern status=<status> not found\n");
        free(line);
        return ERROR;
    }
    
    free(line);
    return SUCCESS;
}

int getOutputFileName(FILE *source, char **value) {
    char *line = NULL;
    char *fileName = NULL;
    size_t sizeOfLine = 0;

    
    errno = SUCCESS;
    if (getline(&line, &sizeOfLine, source) < SUCCESS) {
        free(line);
        if (errno == EINVAL || errno == EOVERFLOW) {
            fprintf(stderr, "getline failed\n");
            return ERROR;
        }
        else {
            /* EOF */
            return DONE;
        }
    }
    
    fileName = malloc(sizeof(char) * strlen(line));
    if (fileName == NULL) {
        free(line);
        return ERROR;
    }
    fileName[0] = '\0';
    if (sscanf(line, "file=%s", fileName) == EOF) {
        fprintf(stderr, "pattern 'file=<filename>' not found\n");
        free(fileName);
        free(line);
        return ERROR;
    }
    
    free(line);
    
    if (strlen(fileName) == 0) {
        fprintf(stderr, "pattern 'file=<filename>' not found\n");
        free(fileName);
        return ERROR;
    }
    
    *value = fileName;
    return SUCCESS;
}

int getOutputFileLength(FILE *source, unsigned long *value) {
    char *line = NULL;
    size_t sizeOfLine = 0;
    int found = 0;
    
    errno = SUCCESS;
    if (getline(&line, &sizeOfLine, source) < SUCCESS) {
        if (errno == EINVAL || errno == EOVERFLOW) {
            fprintf(stderr, "getline failed\n");
            free(line);
            return ERROR;
        }
    }
    
    found = sscanf(line, "len=%lu", value);
    if (found == 0 || found == EOF) {
        fprintf(stderr, "pattern 'len=<lenght>' not found\n");
        free(line);
        return ERROR;
    }
    
    free(line);
    return SUCCESS;
}

int transferFile(FILE *source) {
    char *fileName = NULL;
    unsigned long fileLength = 0;
    int result = 0;
    
    if ((result = getOutputFileName(source, &fileName)) != SUCCESS) return result;
    if ((result = getOutputFileLength(source, &fileLength)) != SUCCESS) return result;
    
    errno = SUCCESS;
    int outputFileDescriptor = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (outputFileDescriptor == ERROR) {
        free(fileName);
        return ERROR;
    }
    
    free(fileName);
    FILE *outputFile = fdopen(outputFileDescriptor, "w");
    if (outputFile == NULL) {
        return ERROR;
    }

    size_t bytesAvailable = 0;
    size_t bytesWritten = 0;
    size_t bytesTransferred = 0;
    size_t bufferSize = 1;
    char buffer;
    
    while ((bytesAvailable = fread(&buffer, (size_t)sizeof(char), bufferSize, source)) > 0) {
        
        bytesWritten = fwrite(&buffer, (size_t)sizeof(char), bytesAvailable, outputFile);
        if (bytesAvailable != bytesWritten) {
            fprintf(stderr, "failed writing %zu bytes to file\n", bytesAvailable);
            fclose(outputFile);
            return ERROR;
        }
        bytesTransferred += bytesWritten;
        if (bytesTransferred == fileLength) {
            fclose(outputFile);
            return SUCCESS;
        }
    }
    
    fclose(outputFile);
    if (bytesTransferred < fileLength) {
        fprintf(stderr, "missing bytes! received %zu out of %lu\n", bytesTransferred, fileLength);
        return ERROR;
    }

    return SUCCESS;
}


void showUsage(FILE *stream, const char *cmnd, int exitcode) {
    fprintf(stream, "%s: %s\n", cmnd, "-s server -p port -u user [-i image URL] -m message [-v] [-h]");
    exit(exitcode);
}

