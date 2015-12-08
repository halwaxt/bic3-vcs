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
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include "simple_message_client_commandline_handling.h"

#define ERROR -1
#define SUCCESS 0
#define DONE 2

/* maximum length for the queue of pending connections */
#define LISTENQ 1024

#define INFO(function, M, ...) \
	if (verbose) fprintf(stdout, "%s [%s, %s, line %d]: " M "\n", programName, __FILE__, function, __LINE__, ##__VA_ARGS__)

static int verbose;
static const char *programName;

void printUsage(const char *programName);
void sigchld_handler(int signo);
int convertString2Int(const char *string);
void handle_client(int cfd);

int main(int argc, const char * argv[]) {

	verbose = 1;
	programName = argv[0];

	int opt = 0;
	int port = -1;
	const char *portstring;

    int sfd = 0;
	int cfd = 0;
    pid_t childpid;
    socklen_t clientLen;
    struct sockaddr_in peerAddress;
	struct sockaddr_in myAddress;
    struct sigaction sa; /* for wait-child-handler */

	INFO("main()", "argc = %d", argc);
	if(argc < 2) {
		printUsage(programName);
	}

	INFO("main()", "start commanline parsing %s", "");
	static struct option long_options[] = {
		{"port", required_argument, 0, 'p'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};
	int long_index =0;
	while ((opt = getopt_long(argc, (char ** const) argv, "p:v", long_options, &long_index)) != -1) {
		switch(opt) {
			case 'p':
				portstring = optarg;
				INFO("getopt_long()", "port=%s", portstring);
				port = convertString2Int(portstring);
				break;
			case 'v':
				verbose = 1;
				break;
			default:
				printUsage(programName);
		}
	}

	INFO("main()", "start creating socket %s", "");
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sfd < 0)
    {
        perror("socket() error..");
		close(sfd);
        exit(EXIT_FAILURE);
    }
	INFO("main()", "socket created %s", "");

	INFO("main()", "preparing connection infos %s", "");
    memset(&myAddress, 0, sizeof(struct sockaddr_in));
    myAddress.sin_family = AF_INET;
    myAddress.sin_port = htons(port); /* bind server to port */
    myAddress.sin_addr.s_addr = htonl(INADDR_ANY); /* bind server to all interfaces */

	INFO("main()", "start binding socket to %d:%u", myAddress.sin_addr.s_addr, port);
    if(bind(sfd, (struct sockaddr*) &myAddress, sizeof(struct sockaddr_in)) != SUCCESS)
    {
        perror("bind() failed..");
        close(sfd);
        exit(EXIT_FAILURE);
    }
	INFO("main()", "binding succeeded %s", "");

	INFO("main()", "start listening with Queue-Size %d", LISTENQ);
    if(listen(sfd, LISTENQ) < 0)
    {
        perror("listen() error..");
        close(sfd);
        exit(EXIT_FAILURE);
    }
	INFO("main()", "listening %s", "");


	INFO("main()", "preparing sigaction() %s", "");
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);


    /* endless listening-loop 
       this loop waits for client-connections.
       if a client is connectd it forks into another
       subprocess.
     */
	INFO("main()", "starting loop for client acception %s", "");
    while(1)
    {   
            clientLen = sizeof(peerAddress);
			INFO("main()", "clientLen = %d", clientLen);

			INFO("main()", "accepting connection from client %s", "");
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

			INFO("main()", "create subprocess: fork() %s", "");
            childpid = fork();
            if(childpid < 0)
            {
                perror("fork() failed");
				close(sfd);
                exit(EXIT_FAILURE);
            }

			INFO("main()", "start cild-subprocess %s", "");
            if( childpid  == 0 )
            {
                close(sfd);
                handle_client(cfd);
            }
			INFO("main()", "client has PID=%i", childpid);

			INFO("main()", "continue server %s", "");

			INFO("main()", "closing client filedescriptor %s", "");
            close(cfd);
    }

    return EXIT_SUCCESS;
}

void printUsage(const char *programName) {
	fprintf(stderr, "usage: %s option:\n", programName);
	fprintf(stderr, "options:\n\t-p, --port <port>\n\t-h, --help\n");
	exit(EXIT_FAILURE);	
}

int convertString2Int(const char *string) {
	INFO("convertString2Int()", "convert port string (%s) to interger", string);

	char *endptr;
	long val;

	errno = 0;

	val = strtol(string, &endptr, 10);

	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
		|| (errno != 0 && val == 0)) {
			perror("strtol");
			exit(EXIT_FAILURE);
	}

	if (endptr == string) {
		fprintf(stderr, "No digits were found\n");
		exit(EXIT_FAILURE);
	}

	INFO("convertString2Int()", "converted string %s to integer %ld", string, val);
	return val;

}

void sigchld_handler(int signo) {
	signo = signo;
    int status;
    pid_t pid;

	INFO("sigchld_handler()", "entered sigchld_handler %s", "");
    /*
       -1 means that we wait until the first process is terminated
       WNOHANG tells the kernel not to block if there are no terminated
       child-processes.
     */
    while( (pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
		INFO("waitpid()", "%i exited with %i\n", pid, WEXITSTATUS(status));
    }
}

void handle_client(int cfd) {

	INFO("handle_client()", "twisting filedescriptors to STDIN and STDOUT %s", "");
	if ((dup2(cfd, STDIN_FILENO) == ERROR) || (dup2(cfd, STDOUT_FILENO) == ERROR)) {
		perror("dub2() failed");
		close(cfd);
		exit(EXIT_FAILURE);
	}

	INFO("handle_client()", "closing client fd %i", cfd);
	if(close(cfd) != SUCCESS) {
		perror("close() failed");
		exit(EXIT_FAILURE);
	}

	INFO("handle_client()", "starting simple_message_server_logic %s", "");
	execl("/usr/local/bin/simple_message_server_logic", "simple_message_server_logic", (char *) NULL);
}
