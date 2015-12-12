//
//  main.c
//  simple_message_server
//
//  Created by Thomas {Halwax,Zeitinger} on 24.11.15.
//  Copyright © 2015 Technikum Wien. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>

#define ERROR -1
#define SUCCESS 0
#define DONE 2

/* maximum length for the queue of pending connections */
#define BACKLOG_SIZE 10

/* handling interaction with clients */
#define PATH_TO_SERVER_LOGIC "/usr/local/bin/simple_message_server_logic"
#define SERVER_LOGIC "simple_message_server_logic"

#define INFO(function, M, ...) \
	if (verbose) fprintf(stdout, "%s [%s, %s, line %d]: " M "\n", programName, __FILE__, function, __LINE__, ##__VA_ARGS__)

//static int verbose;
static const char *programName;
static int verbose = 1;

void printUsage();
void handleChildSignals(int signalNumber);
void waitForClients(int listening_socket_descriptor);
void startClientInteraction(int client_socket_descriptor);
const char *getTcpPort(int argc, const char *argv[]);

int main(int argc, const char * argv[]) {
    
    INFO("main()", "argc = %d", argc);
    if(argc < 2) {
        printUsage(programName);
        exit(EXIT_FAILURE);
    }
    
    programName = argv[0];
    const char *tcpPort = getTcpPort(argc, argv);
    if (tcpPort == NULL) {
        exit(EXIT_FAILURE);
    }
    
    INFO("main()", "using tcp port %s", tcpPort);
    
    struct addrinfo *addrInfoResult, hints;
    memset(&hints, 0, sizeof(hints));
    
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    int result;
    if ((result = getaddrinfo(NULL, tcpPort, &hints, &addrInfoResult)) != SUCCESS)
    {
        fprintf(stderr, "%s: getaddrinfo(): %s\n", programName, gai_strerror(result));
        exit(EXIT_FAILURE);
    }
    
    INFO("main()", "getaddrinfo succeeded %s", "");
    
    int listening_socket_descriptor = 0;
    struct addrinfo *serverCandidate;
    int optionValue = 1;
    
    /* we use the first addrinfo which works */
    for (serverCandidate = addrInfoResult; serverCandidate != NULL; serverCandidate = serverCandidate->ai_next) {
        listening_socket_descriptor = socket(serverCandidate->ai_family,
                       serverCandidate->ai_socktype, serverCandidate->ai_protocol);
        
        if (listening_socket_descriptor == ERROR) {
            INFO("main()", "failed creating a socket for %d, %d, %d", serverCandidate->ai_family, serverCandidate->ai_socktype, serverCandidate->ai_protocol);
            continue;
        }
        
        /* Setting the SO_REUSEADDR option means that we can bind a socket to a local port even if another TCP is bound to 
         the same port in either of the scenarios described at the start of this section. Most TCP servers should enable 
         this option.
         The Linux Programming Interface , p1280
         */
        if (setsockopt(listening_socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &optionValue, sizeof(int)) == ERROR) {
            fprintf(stderr, "%s: getaddrinfo(): %s\n", programName, gai_strerror(result));
            freeaddrinfo(serverCandidate);
            freeaddrinfo(addrInfoResult);
            close(listening_socket_descriptor);
            exit(EXIT_FAILURE);
        }
        
        if (bind(listening_socket_descriptor, serverCandidate->ai_addr, serverCandidate->ai_addrlen) == ERROR) {
            /* could not bind, try next addrInfo */
            INFO("main()", "failed to bind %s", "");
            continue;
        }
        
        /* if we came here we had no error, so let's go on
        */
        break;
    }

    INFO("main()", "freeing addrInfoResult %s", "");
    freeaddrinfo(addrInfoResult);
    
    if (serverCandidate == NULL) {
        fprintf(stderr, "%s: failed to bind: %s\n", programName, strerror(errno));
        close(listening_socket_descriptor);
        exit(EXIT_FAILURE);
    }

    INFO("main()", "start listening %s", "");
    
    if (listen(listening_socket_descriptor, BACKLOG_SIZE) == ERROR) {
        fprintf(stderr, "%s: failed to listen: %s\n", programName, strerror(errno));
        close(listening_socket_descriptor);
        exit(EXIT_FAILURE);
    }
    
    INFO("main()", "attaching signal handler %s", "");
    struct sigaction onSignalAction;
    memset(&onSignalAction, 0, sizeof(onSignalAction));
    
    onSignalAction.sa_handler = handleChildSignals;
    onSignalAction.sa_flags = SA_RESTART;
    
    /* register handler for SIGCHLD: child status has changed */
    if (sigaction(SIGCHLD, &onSignalAction, NULL) == ERROR) {
        fprintf(stderr, "%s: failed to create signal handler: %s\n", programName, strerror(errno));
        close(listening_socket_descriptor);
        exit(EXIT_FAILURE);
    }
    
    waitForClients(listening_socket_descriptor);
}

void waitForClients(int listening_socket_descriptor) {
    int client;
    socklen_t addressSize;
    struct sockaddr_storage clientAddress;

    INFO("waitForClients()", "waiting for client connections %s", "");
    while (1 == 1) {
        addressSize = sizeof(clientAddress);
        client = accept(listening_socket_descriptor, (struct sockaddr *)&clientAddress, &addressSize);
        INFO("waitForClients()", "accepted client %s", "");
        if (client < SUCCESS) {
            if (errno != EINTR) {
                fprintf(stderr, "%s: failed to accept client: %s\n", programName, strerror(errno));
                close(listening_socket_descriptor);
                exit(EXIT_FAILURE);
            }
            else {
                /* handle next one, we've been interrupted by a signal */
                INFO("waitForClients()", "interrupted by signal %s", "");
                continue;
            }
        }
        
        INFO("waitForClients()", "forking %s", "");
        switch (fork()) {
            case ERROR: {
                fprintf(stderr, "%s: failed to fork: %s\n", programName, strerror(errno));
                close(client);
                close(listening_socket_descriptor);
                exit(EXIT_FAILURE);
            }
            /* child process */
            case SUCCESS:
                if (close(listening_socket_descriptor) != SUCCESS) {
                    fprintf(stderr, "%s: failed to close listening fd: %s\n", programName, strerror(errno));
                    close(client);
                    exit(EXIT_FAILURE);
                }
                startClientInteraction(client);
                break;
            /* parent process */
            default: {
                if (close(client) != SUCCESS) {
                    fprintf(stderr, "%s: failed to close client fd: %s\n", programName, strerror(errno));
                    close(listening_socket_descriptor);
                    exit(EXIT_FAILURE);
                }
                break;
            }
        }
    }
}

void startClientInteraction(int client) {
    /* connect STDIN and STDOUT to client */
    if (dup2(client, STDIN_FILENO) == ERROR || dup2(client, STDOUT_FILENO) == ERROR) {
        fprintf(stderr, "%s: failed to duplicate fd for client connection: %s\n", programName, strerror(errno));
        close(client);
        exit(EXIT_FAILURE);
    }
    
    if (close(client) != SUCCESS) {
        fprintf(stderr, "%s: failed to close client connection: %s\n", programName, strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    (void)execl(PATH_TO_SERVER_LOGIC, SERVER_LOGIC, (char *)NULL);
    
    /* if execl fails */
    _exit(127);
}


void handleChildSignals(int signalNumber)
{
    int status;
    pid_t pid;
    
    /* get rid of compiler warnings */
    signalNumber = signalNumber;
  
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* do something very clever while waiting for process to exit :-) */
    }
}

const char *getTcpPort(int argc, const char *argv[]) {
    
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    char *tcpPort = NULL;
    int opt = 0;
    int long_index =0;
    
    while ((opt = getopt_long(argc, (char ** const) argv, "p:v", long_options, &long_index)) != -1) {
        switch(opt) {
            case 'p':
                tcpPort = optarg;
                INFO("getTcpPort()", "tcp port=%s", tcpPort);
            default:
                printUsage();
        }
    }
    
    return tcpPort;
}

void printUsage() {
    fprintf(stderr, "usage: %s option:\n", programName);
    fprintf(stderr, "options:\n\t-p, --port <port>\n\t-h, --help\n");
    exit(EXIT_FAILURE);
}

    