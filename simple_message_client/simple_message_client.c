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
#include <stdarg.h>
#include "simple_message_client_commandline_handling.h"

#define ERROR -1
#define SUCCESS 0
#define DONE 2

#define log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

void showUsage(FILE *stream, const char *cmnd, int exitcode);
int write_formatted(const char *formatted_string, ...);

int connetToServer(const char *server, const char *port, int *socketDescriptor);
int sendData(FILE *target, const char *key, const char *payload);
int checkServerResponseStatus(FILE *source, int *status);
int transferFile(FILE *source);
int getOutputFileLength(FILE *source, unsigned long *value);
int getOutputFileName(FILE *source, char **value);

static const char *programName;
static int verbose;

int main(int argc, const char * argv[]) {

    const char *server;
    const char *port;
    const char *user;
    const char *image_url;
    const char *message;

    programName = argv[0];
    
    smc_parsecommandline(argc, argv, showUsage, &server, &port, &user, &message, &image_url, &verbose);

    log_info("command line args: server='%s', port='%s', user='%s', img_url='%s', message=%message'\n", server, port, user, message);
    
    int sfd = 0;
    if (connetToServer(server, port, &sfd) != SUCCESS) {
        fprintf(stderr, "%s: connectToServer() failed for server %s and port %s: %s\n", programName,   server, port, strerror(errno));
        exit(errno);
    }
    log_info("connected to %s:%s\n", server, port);
    
    errno = SUCCESS;
    FILE *toServer = fdopen(sfd, "w");
    if (toServer == NULL) {
        fprintf(stderr, "%s: fdOpen() to write to server failed: %s\n", programName, strerror(errno));
        close(sfd);
        exit(errno);
    }
    
    log_info("sending 'user=%s'\n", user);
    if (sendData(toServer, "user=", user) == ERROR) {
        fprintf(stderr, "%s: sendData() for param user=<user> failed: %s\n", programName, strerror(errno));
        shutdown(sfd, SHUT_RDWR);
        fclose(toServer);
        exit(errno);
    }
    
    if (image_url != NULL) {
        log_info("sending 'img=%s'\n", image_url);
        if (sendData(toServer, "img=", image_url) == ERROR) {
            fprintf(stderr, "%s: sendData() for param img=<image_url> failed: %s\n", programName, strerror(errno));
            shutdown(sfd, SHUT_RDWR);
            fclose(toServer);
            exit(errno);
        }
    }
    else {
        log_info("skipping img=<image_url> parameter");
    }
    
    log_info("sending message '%s'\n", message);
    if (sendData(toServer, "", message) == ERROR) {
        fprintf(stderr, "%s: sendData() for message failed: %s\n", programName, strerror(errno));
        shutdown(sfd, SHUT_RDWR);
        fclose(toServer);
        exit(errno);
    }
    
    /* fclose schließt auch sfd, daher vorher ein dup */
    
    int backupOfSfd = dup(sfd);
    log_info("shutting down file pointer for writing socket\n");
    if (shutdown(sfd, SHUT_WR) != SUCCESS) {
        fprintf(stderr, "%s: shutDown() SHUT_WR for server connection failed: %s\n", programName, strerror(errno));
        fclose(toServer);
        exit(EXIT_FAILURE);
    }
    log_info("closing writing socket\n");
    fclose(toServer);
    
    log_info("opening reading socket\n");
    FILE *fromServer = fdopen(backupOfSfd, "r");
    if (fromServer == NULL) {
        fprintf(stderr, "%s: fdOpen() to read from server failed: %s\n", programName, strerror(errno));
        close(backupOfSfd);
        exit(errno);
    }
    
    /* read line for status=... */
    /* if status returned from server != 0 then exit using the status */
    log_info("reading server response\n");
    int status = ERROR;
    if (checkServerResponseStatus(fromServer, &status) != SUCCESS || status != SUCCESS) {
        fprintf(stderr, "%s: reading server response failed with error %d\n", programName, status);
        fclose(fromServer);
        close(backupOfSfd);
        exit(status);
    }
    
    log_info("downloading files\n");
    int canTransferFile = SUCCESS;
    while (canTransferFile != DONE) {
        canTransferFile = transferFile(fromServer);
        if (canTransferFile == ERROR) {
            fprintf(stderr, "%s: transferFile() failed: %s\n", programName, strerror(errno));
            fclose(fromServer);
            close(backupOfSfd);
            exit(EXIT_FAILURE);
        }
    }
    
    log_info("shutting down reading socket\n");
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
        fprintf(stderr, "%s: getaddrinfo() failed: %s\n", programName, strerror(errno));
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
        fprintf(stderr, "%s: could not connect: %s\n", programName, strerror(errno));
        return ERROR;
    }
    
    freeaddrinfo(rp);
    *socketDescriptor = sfd;
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
        fprintf(stderr, "%s: checkServerResponseStatus()/getline() failed: %s\n", programName, strerror(errno));
            return ERROR;
        }
        else {
            /* must be EOF */
            return DONE;
        }
    }

    found = sscanf(line, "status=%d", status);
    if (found == 0 || found == EOF) {
        fprintf(stderr, "%s: checkServerResponseStatus()/sscanf() failed: %s\n", programName, strerror(errno));
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
        fprintf(stderr, "%s: getOutputFileName()/getline() failed: %s\n", programName, strerror(errno));
            return ERROR;
        }
        else {
            /* EOF */
            return DONE;
        }
    }
    
    fileName = malloc(sizeof(char) * strlen(line));
    if (fileName == NULL) {
        fprintf(stderr, "%s: getOutputFileName()/malloc() failed: %s\n", programName, strerror(errno));
        free(line);
        return ERROR;
    }
    fileName[0] = '\0';
    if (sscanf(line, "file=%s", fileName) == EOF) {
        fprintf(stderr, "%s: getOutputFileName()/file=<file> pattern not found\n", programName);
        free(fileName);
        free(line);
        return ERROR;
    }
    
    free(line);
    
    if (strlen(fileName) == 0) {
        fprintf(stderr, "%s: getOutputFileName()/file=<file> pattern not found\n", programName);
        free(fileName);
        return ERROR;
    }
    log_info("output file name is %s\n", fileName);
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
        fprintf(stderr, "%s: getOutputFileLength()/getline() failed: %s\n", programName, strerror(errno));
            free(line);
            return ERROR;
        }
    }
    
    found = sscanf(line, "len=%lu", value);
    if (found == 0 || found == EOF) {
        fprintf(stderr, "%s: getOutputFileLength()/pattern len=<length> not found\n", programName);
        free(line);
        return ERROR;
    }
    free(line);
    log_info("ouput file size is %lu bytes\n", *value);
    return SUCCESS;
}

int transferFile(FILE *source) {
    char *fileName = NULL;
    unsigned long fileLength = 0;
    int result = 0;
    
    if ((result = getOutputFileName(source, &fileName)) != SUCCESS) return result;
    if ((result = getOutputFileLength(source, &fileLength)) != SUCCESS) return result;
    
    errno = SUCCESS;
    log_info("opening file %s for writing\n", fileName);
    int outputFileDescriptor = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (outputFileDescriptor == ERROR) {
        fprintf(stderr, "%s: transferFile()/open() failed: %s\n", programName, strerror(errno));
        free(fileName);
        return ERROR;
    }
    
    free(fileName);
    FILE *outputFile = fdopen(outputFileDescriptor, "w");
    if (outputFile == NULL) {
        fprintf(stderr, "%s: transferFile()/fdopen() failed: %s\n", programName, strerror(errno));
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
            fprintf(stderr, "%s: failed writing %zu bytes to file\n", programName, bytesAvailable);
            fclose(outputFile);
            return ERROR;
        }
        bytesTransferred += bytesWritten;
        if (bytesTransferred == fileLength) {
            log_info("transfered of %lu completed\n", fileLength);
            fclose(outputFile);
            return SUCCESS;
        }
    }
    
    log_info("closing output file %s\n", fileName);
    fclose(outputFile);
    if (bytesTransferred < fileLength) {
        fprintf(stderr, "%s: missing bytes! received %zu out of %lu\n", programName, bytesTransferred, fileLength);
        return ERROR;
    }

    return SUCCESS;
}

/**
 * \brief This function prints a formatted string using vprintf
 *
 * \param formatted_string
 *
 * \return returns EXIT or CONTINUE
 * \retval return_code EXIT on failure
 * \retval return_code CONTÍNUE on success
 */
int write_formatted(const char *formatted_string, ...) {
    
    int return_code = SUCCESS;
    
    va_list args;
    
    va_start(args, formatted_string);
    if (vprintf(formatted_string, args) < 0) {
        fprintf(stderr, "%s: write_formatted(): %s\n", programName, strerror(errno));
        return_code = ERROR;
    }
    va_end(args);
    
    return return_code;
}


void showUsage(FILE *stream, const char *cmnd, int exitcode) {
    fprintf(stream, "%s: %s\n", cmnd, "-s server -p port -u user [-i image URL] -m message [-v] [-h]");
    exit(exitcode);
}

