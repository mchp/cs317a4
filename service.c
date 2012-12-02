/*
 * File: service.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "service.h"
#include "util.h"

void handle_client(int socket) {
    
	int len = 10000;
	int curr_len = 0;
	char* buffer = malloc(len);
	while(http_header_complete(buffer, curr_len) == -1) {
		curr_len += recv(socket, buffer+curr_len, len-curr_len,0);	
		printf(buffer);
		printf("curr len %d\n", curr_len);
	}


	char* cookie_value = http_parse_header_field(buffer, strlen("Connection"), "Connection");

	send(socket,buffer, strlen(buffer), 0);
	
}    
