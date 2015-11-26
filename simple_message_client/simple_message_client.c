//
//
//  simple_message_client
//
//  Created by Thomas {Halwax,Zeitinger} on 24.11.15.
//  Copyright © 2015 Technikum Wien. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include "simple_message_client_commandline_handling.h"

void showUsage(FILE *stream, const char *cmnd, int exitcode);

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
    return 0;
}


void showUsage(FILE *stream, const char *cmnd, int exitcode) {
    fprintf(stream, "%s: %s\n\n", cmnd, "invalid params");
    fprintf(stream, "%s: %s\n", cmnd, "-s server -p port -u user [-i image URL] -m message [-v] [-h]");
    exit(exitcode);
}






