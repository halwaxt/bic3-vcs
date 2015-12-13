/* vim: set ts=4 sw=4 sts=4 et : */
/**
 * @file simple_message_client_cs.c
 * VCS - Tcp/Ip Exercise - client program connects to simple_message_server on a
 * given port, and sends bulletin board messages. After sending, connection is 
 * shutdown and response is stored local.
 *
 * @author Thomas Halwax <ic14b050@technikum-wien.at>
 * @author Thomas Zeitinger <ic14b033@technikum-wien.at>
 * @date 2015/12/13
 *
 * @version $Revision: 1.0 $
 *
 * @todo everything is done
 *
 * URL: $HeadURL$
 *
 * Last Modified: $Author: thomas $
 */

/*
 * --------------------------------------------------------------- includes --
 */

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

/*
 * ---------------------------------------------------------------- defines --
 */

#define ERROR -1
#define SUCCESS 0
#define DONE 2

#define INFO(function, M, ...) \
		if (verbose) fprintf(stdout, "%s [%s, %s, line %d]: " M "\n", programName, __FILE__, function, __LINE__, ##__VA_ARGS__)

/*
 * ---------------------------------------------------------------- globals --
 */

static const char *programName;
static int verbose;

/*
 * ------------------------------------------------------------- prototypes --
 */

void showUsage(FILE *stream, const char *cmnd, int exitcode);
static int connectToServer(const char *server, const char *port, int *socketDescriptor);
static int sendData(FILE *target, const char *key, const char *payload);
static int checkServerResponseStatus(FILE *source, int *status);
static int transferFile(FILE *source);
static int getOutputFileLength(FILE *source, unsigned long *value);
static int getOutputFileName(FILE *source, char **value);

/**
 * @brief       Main function
 *
 * This function is the main entry point of the program. -
 * it opens a connection to specified server and sends and receives data
 *
 * \param argc the number of arguments
 * \param argv the arguments itselves (including the program name in argv[0])
 *
 * @return    exit status of program
 * @retval    EXIT_FAILURE      Program terminated due to a failure
 * @retval    EXIT_SUCCESS      Program terminated successfully
 *
 */

int main(int argc, const char * argv[]) {

    const char *server;
    const char *port;
    const char *user;
    const char *image_url;
    const char *message;
    
    programName = argv[0];
    
    smc_parsecommandline(argc, argv, showUsage, &server, &port, &user, &message, &image_url, &verbose);
    
    INFO("main()", "Using the following options: server=\"%s\", port=\"%s\", user=\"%s\", img_url=\"%s\", message=\"%s\"", server, port, user, image_url, message);
	
    INFO("main()", "connecting to server=\"%s\", port=\"%s\"", server, port);
    int sfd = 0;
    if (connectToServer(server, port, &sfd) != SUCCESS) {
        fprintf(stderr, "%s: connectToServer() failed for server %s and port %s: %s\n", programName, server, port, strerror(errno));
        exit(errno);
    }
    
    INFO("main()", "open file descriptor for writing %s", "");
    errno = SUCCESS;
    FILE *toServer = fdopen(sfd, "w");
    if (toServer == NULL) {
        fprintf(stderr, "%s: fdOpen() to write to server failed: %s\n", programName, strerror(errno));
        close(sfd);
        exit(errno);
    }
    
	INFO("main()", "sending data to server %s", server);
    if (sendData(toServer, "user=", user) == ERROR) {
        fprintf(stderr, "%s: sendData() for param user=<user> failed: %s\n", programName, strerror(errno));
        shutdown(sfd, SHUT_RDWR);
        fclose(toServer);
        exit(errno);
    }
    
    if (image_url != NULL) {
		INFO("main()", "found image, sending to server %s", server);
        if (sendData(toServer, "img=", image_url) == ERROR) {
            fprintf(stderr, "%s: sendData() for param img=<image_url> failed: %s\n", programName, strerror(errno));
            shutdown(sfd, SHUT_RDWR);
            fclose(toServer);
            exit(errno);
        }
    }
    
	INFO("main()", "send message to server %s", server);
    if (sendData(toServer, "", message) == ERROR) {
        fprintf(stderr, "%s: sendData() for message failed: %s\n", programName, strerror(errno));
        shutdown(sfd, SHUT_RDWR);
        fclose(toServer);
        exit(errno);
    }
    
    /* fclose schließt auch sfd, daher vorher ein dup */
    INFO("main()", "creating backup of file descriptor %s", "");	
    int backupOfSfd = dup(sfd);
    
	INFO("main()", "closing connection to server %s", server);
    if (shutdown(sfd, SHUT_WR) != SUCCESS) {
        fprintf(stderr, "%s: shutDown() SHUT_WR for server connection failed: %s\n", programName, strerror(errno));
        fclose(toServer);
        exit(EXIT_FAILURE);
    }
    
	INFO("main()", "closing file descriptor %s", "");
    fclose(toServer);
    INFO("main()", "closed writing channel to server %s", server);
    
	INFO("main()", "open stream from server %s", server);
    FILE *fromServer = fdopen(backupOfSfd, "r");
    if (fromServer == NULL) {
        fprintf(stderr, "%s: fdOpen() to read from server failed: %s\n", programName, strerror(errno));
        close(backupOfSfd);
        exit(errno);
    }
    INFO("main()", "opened reading channel from server %s", server);
    /* read line for status=... */
    /* if status returned from server != 0 then exit using the status */
    int status = ERROR;
    
	INFO("main()", "start checking server response %s", "");
    if (checkServerResponseStatus(fromServer, &status) != SUCCESS || status != SUCCESS) {
        fprintf(stderr, "%s: reading server response failed with error %d\n", programName, status);
        fclose(fromServer);
        close(backupOfSfd);
        exit(status);
    }
    INFO("main()", "server returned status %d", status);
    
	INFO("main()", "start receiving files from server %s", server);
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
    
	INFO("main()", "received all data, closing connection to server %s", server);
    fclose(fromServer);
    close(backupOfSfd);
    INFO("main()", "closed connection to server %s", server);
    INFO("main()", "bye %s!", user);
    exit(status);
}

/**
 * @brief connectToServer
 *
 * Connect to server
 * Writes Errors to stderr
 *
 * \param server server address for connecting to
 * \param port port from the server is given
 * \param socketDescriptor file descriptor for handling the connection
 *
 * \return int
 * \retval SUCCESS on Success
 * \retval ERROR on Error
 *
 */

static int connectToServer(const char *server, const char *port, int *socketDescriptor) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd = -1;
    
	INFO("connectToServer()", "creating address strct %s", "");
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Stream socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */
    
	INFO("connectToServer()", "use getaddrinfo() on %s", server);
    if (getaddrinfo(server, port, &hints, &result) != SUCCESS) {
        fprintf(stderr, "%s: getaddrinfo() failed: %s\n", programName, strerror(errno));
        return ERROR;
    }
    
    /* getaddrinfo() returns a list of address structures.
     Try each address until we successfully connect(2).
     If socket(2) (or connect(2)) fails, we (close the socket
     and) try the next address. */
 	INFO("connectToServer()", "iterate through list of getaddrinfo filled structure %s", ""); 
    for (rp = result; rp != NULL; rp = rp->ai_next) {
		INFO("connectToServer()", "try socket %d - %d - %d", rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;
        
		INFO("connectToServer()", "try connect to former created socket %s", "");
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */
        
		INFO("connectToServer()", "closing socket %s", "");
        close(sfd);
    }

	INFO("connectToServer()", "freeaddrinfo() %s", "");
    freeaddrinfo(result);           /* No longer needed */
    
    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "%s: could not connect: %s\n", programName, strerror(errno));
        return ERROR;
    }
    
    INFO("connectToServer()", "connected to address %s, port number %s", server, port);
    *socketDescriptor = sfd;
    return SUCCESS;

}

/**
 * @brief sendData
 *
 * Send data to opend connection
 *
 * \param FILE opened file for writing to
 * \param key keyvalue from communication protocol
 * \param payload data
 *
 * \return int
 * \retval SUCCESS on Success
 * \retval ERROR on Error
 *
 */
static int sendData(FILE *target, const char *key, const char *payload) {
    if (fprintf(target, "%s", key) < 0) return ERROR;
    if (fprintf(target, "%s", payload) < 0) return ERROR;
    if (fprintf(target, "\n") < 0) return ERROR;
    if (fflush(target) == EOF) return ERROR;
    INFO("sendData()", "sent Data %s%s", key, payload);
    return SUCCESS;
}

/**
 * @brief checkServerResponseStatus
 *
 * searching key "status=" in data from server
 *
 * \param source opened file for reading from
 * \param status pointer for writing status into
 *
 * \return int
 * \retval DONE on EOF
 * \retval SUCCESS on Success
 * \retval ERROR on Error
 *
 */
static int checkServerResponseStatus(FILE *source, int *status) {
    /* read line from source */
    /* compare n chars with status=0 */
    char *line = NULL;
    size_t sizeOfLine = 0;
    int found = 0;

	INFO("checkServerResponseStatus()", "start read lines %s", "");
    errno = SUCCESS;
    if (getline(&line, &sizeOfLine, source) < SUCCESS) {
        free(line);
        if (errno == EINVAL || errno == EOVERFLOW) {
        fprintf(stderr, "%s: checkServerResponseStatus()/getline() failed: %s\n", programName, strerror(errno));
            return ERROR;
        }
        else {
            /* must be EOF */
            INFO("checkServerResponseStatus()", "found EOF %s", "");
            return DONE;
        }
    }

	INFO("checkServerResponseStatus()", "try to find status code in stream %s", "");
    found = sscanf(line, "status=%d", status);
    if (found == 0 || found == EOF) {
        fprintf(stderr, "%s: checkServerResponseStatus()/sscanf() failed: %s\n", programName, strerror(errno));
        free(line);
        return ERROR;
    }
    free(line);
    
    INFO("checkServerResponseStatus()", "status=%d", *status);
    return SUCCESS;
}

/**
 * @brief getOutputFileName
 *
 * searching key "file=" in data from server
 *
 * \param source opened file for reading from
 * \param value pointer for writing filename into
 *
 * \return int
 * \retval DONE on EOF
 * \retval SUCCESS on Success
 * \retval ERROR on Error
 *
 */
static int getOutputFileName(FILE *source, char **value) {
    char *line = NULL;
    char *fileName = NULL;
    size_t sizeOfLine = 0;
    
	INFO("getOutputFileName()", "start read lines %s", "");
    errno = SUCCESS;
    if (getline(&line, &sizeOfLine, source) < SUCCESS) {
        free(line);
        if (errno == EINVAL || errno == EOVERFLOW) {
        fprintf(stderr, "%s: getOutputFileName()/getline() failed: %s\n", programName, strerror(errno));
            return ERROR;
        }
        else {
            /* EOF */
            INFO("getOutputFileName()", "found EOF %s", "");
            return DONE;
        }
    }
    
	INFO("getOutputFileName()", "try to malloc for filename %s", "");
    fileName = malloc(sizeof(char) * strlen(line));
    if (fileName == NULL) {
        fprintf(stderr, "%s: getOutputFileName()/malloc() failed: %s\n", programName, strerror(errno));
        free(line);
        return ERROR;
    }
    fileName[0] = '\0';
	INFO("getOutputFileName()", "try to find filename in stream %s", "");
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
    INFO("getOutputFileName()", "found fileName %s", fileName);
    /* memory gets freed in transferFile() */
    *value = fileName;
    return SUCCESS;
}

/**
 * @brief getOutputFileLength
 *
 * searching key "len=" in data from server
 *
 * \param source opened file for reading from
 * \param value pointer for writing lenght into
 *
 * \return int
 * \retval DONE on EOF
 * \retval SUCCESS on Success
 * \retval ERROR on Error
 *
 */
static int getOutputFileLength(FILE *source, unsigned long *value) {
    char *line = NULL;
    size_t sizeOfLine = 0;
    int found = 0;
    
	INFO("getOutputFileLength()", "start read lines %s", "");
    errno = SUCCESS;
    if (getline(&line, &sizeOfLine, source) < SUCCESS) {
        if (errno == EINVAL || errno == EOVERFLOW) {
            fprintf(stderr, "%s: getOutputFileLength()/getline() failed: %s\n", programName, strerror(errno));
            free(line);
            return ERROR;
        }
        else {
            /* EOF */
            INFO("getOutputFileLength()", "found EOF %s", "");
            return DONE;
        }
    }
    
	INFO("getOutputFileLength()", "try to find file length in stream %s", "");
    found = sscanf(line, "len=%lu", value);
    if (found == 0 || found == EOF) {
        fprintf(stderr, "%s: getOutputFileLength()/pattern len=<length> not found\n", programName);
        free(line);
        return ERROR;
    }
    
    free(line);
    INFO("getOutputFileLength()", "found len=%lu", *value);
    return SUCCESS;
}

/**
 * @brief transferFile
 *
 * writing data from server to local file
 *
 * \param source opened file for reading from
 *
 * \return int
 * \retval SUCCESS on Success
 * \retval ERROR on Error
 *
 */
static int transferFile(FILE *source) {
    char *fileName = NULL;
    unsigned long fileLength = 0;
    int result = 0;
    
	INFO("transferFile()", "get result from getOutputFileName() %s", "");
    if ((result = getOutputFileName(source, &fileName)) != SUCCESS) return result;
	INFO("transferFile()", "get result from getOutputFileLength() %s", "");
    if ((result = getOutputFileLength(source, &fileLength)) != SUCCESS) return result;
    
    errno = SUCCESS;
	INFO("transferFile()", "open outputFileDescriptor %s", "");
    int outputFileDescriptor = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (outputFileDescriptor == ERROR) {
        fprintf(stderr, "%s: transferFile()/open() failed: %s\n", programName, strerror(errno));
        free(fileName);
        return ERROR;
    }

	INFO("transferFile()", "fdopen outputFile  %s", "");
    FILE *outputFile = fdopen(outputFileDescriptor, "w");
    if (outputFile == NULL) {
        free(fileName);
        fprintf(stderr, "%s: transferFile()/fdopen() failed: %s\n", programName, strerror(errno));
        return ERROR;
    }
    INFO("transferFile()", "opened %s for writing", fileName);
    free(fileName);

    size_t bytesAvailable = 0;
    size_t bytesWritten = 0;
    size_t bytesTransferred = 0;
    size_t bufferSize = 1;
    char buffer;
    
	INFO("transferFile()", "start writing bytes to outputFile %s", fileName);
    while ((bytesAvailable = fread(&buffer, (size_t)sizeof(char), bufferSize, source)) > 0) {
        
        bytesWritten = fwrite(&buffer, (size_t)sizeof(char), bytesAvailable, outputFile);
        if (bytesAvailable != bytesWritten) {
            fprintf(stderr, "%s: failed writing %zu bytes to file\n", programName, bytesAvailable);
            fclose(outputFile);
            return ERROR;
        }
        bytesTransferred += bytesWritten;
        if (bytesTransferred == fileLength) {
            INFO("transferFile()", "transferred %zu bytes to file", bytesTransferred);
            fclose(outputFile);
            return SUCCESS;
        }
    }
    
    fclose(outputFile);
    INFO("transferFile()", "closed %s", fileName);
    if (bytesTransferred < fileLength) {
        fprintf(stderr, "%s: missing bytes! received %zu out of %lu\n", programName, bytesTransferred, fileLength);
        return ERROR;
    }

    return SUCCESS;
}

/**
 * @brief showUsage
 *
 * print usage parameter
 *
 * \param stream stream to write the usage information to
 * \param cmnd a string containing the name of the executable
 * \param exitcode the  exit code to be used in the call to exit(3) for terminating the program
 *
 * \return int
 * \retval SUCCESS on Success
 * \retval ERROR on Error
 *
 */
void showUsage(FILE *stream, const char *cmnd, int exitcode) {
    fprintf(stream, "%s: %s\n", cmnd, "-s server -p port -u user [-i image URL] -m message [-v] [-h]");
    exit(exitcode);
}

