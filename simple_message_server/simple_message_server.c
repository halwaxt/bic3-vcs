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

void usage(const char *programName);

static const char *programName;


/* maxium of allowed clients at the same time */
#define MAX_CLIENTS 10

/* maximum length for the queue of pending connections */
#define LISTENQ 1024

/*
   our child-handler. wait for all children to avoid zombies
 */
/* TODO: WTF is signo? */
void sigchld_handler(int signo)
{
    int status;
    pid_t pid;

    /*
       -1 means that we wait until the first process is terminated
       WNOHANG tells the kernel not to block if there are no terminated
       child-processes.
     */
    while( (pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        fprintf(stdout, "%i exited with %i\n", pid, WEXITSTATUS(status));
    }

    return;
}


int main(int argc, const char * argv[]) {

	programName = argv[0];

	int opt = -1;
	const char *port;

    int sfd = 0;
	int cfd = 0;
    pid_t childpid;
    socklen_t clientLen;
    struct sockaddr_in peerAddress;
	struct sockaddr_in myAddress;
    struct sigaction sa; /* for wait-child-handler */

	/* Commandline parsen */
	while ((opt = getopt(argc, (char ** const) argv, "p:h")) != -1) {
		switch(opt) {
			case 'p':
				port = optarg;
				break;
			default:
				usage(programName);
				exit(EXIT_FAILURE);	
		}
	}

	/* TODO: getaddrinfo */
    /* Create Server-Socket */
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sfd < 0)
    {
        perror("socket() error..");
		close(sfd);
        exit(EXIT_FAILURE);
    }

    /* Prepare our address-struct */
    memset(&myAddress, 0, sizeof(struct sockaddr_in));
    myAddress.sin_family = AF_INET;
    myAddress.sin_port = htons(port); /* bind server to port */
    myAddress.sin_addr.s_addr = htonl(INADDR_ANY); /* bind server to all interfaces */

    /* bind sockt to address + port */
    if(bind(sfd, (struct sockaddr*) &myAddress, sizeof(struct sockaddr_in)) != 0)
    {
        perror("bind() failed..");
        close(sfd);
        exit(EXIT_FAILURE);
    }

    /* start listening */
    if(listen(sfd, LISTENQ) < 0)
    {
        perror("listen() error..");
        close(sfd);
        exit(EXIT_FAILURE);
    }


    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);


    /* endless listening-loop 
       this loop waits for client-connections.
       if a client is connectd it forks into another
       subprocess.
     */
    while(1)
    {   
            clientLen = sizeof(peerAddress);
            /* accept connections from clients */
            cfd = accept(sfd, (struct sockaddr *) &peerAddress, &clientLen);
            if(cfd < 0)
            {
                /*
                   withouth this we would recieve:
                "accept() error..: Interrupted system call" after
                each client disconnect. this happens because SIGCHLD
                blocks our parent-accept(). 
                 */
                if (errno == EINTR)
                    continue;

                    perror("accept() error..");
                    close(sfd);
                    exit(EXIT_FAILURE);
            }

            /* lets create a subprocess */
            childpid = fork();
            if(childpid < 0)
            {
                perror("fork() failed");
                exit(EXIT_FAILURE);
            }

            /* let's start our child-subprocess */
            if( childpid  == 0 )
            {
                close(sfd);
				/* TODO: was starten wir? */
                exit(handle_client(cfd));
            }

            /* continue our server-routine */
            fprintf(stdout, "Client has PID %i\n",childpid);

            close(cfd);
    }

    return EXIT_SUCCESS;
}

void usage(programName) {
	fprintf(stdout, "%s: fooo", programName);
}
