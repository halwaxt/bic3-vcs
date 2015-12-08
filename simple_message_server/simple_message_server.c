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
#include <signal.h>

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
        INFO("found param %s", opt);
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

    
    return EXIT_SUCCESS;
}

void printUsage(const char *programName) {
	fprintf(stderr, "usage: %s option:\n", programName);
	fprintf(stderr, "options:\n\t-p, --port <port>\n\t-h, --help\n");
	exit(EXIT_FAILURE);	
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

