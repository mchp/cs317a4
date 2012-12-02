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


void handle_client(int socket) {
    
	int len = 10000;
	int curr_len = 0;
	char* buffer = malloc(len);
	request_info request;
	response_info response;
	while(http_header_complete(buffer, curr_len) == -1) {
		curr_len += recv(socket, buffer+curr_len, len-curr_len,0);	
		printf(buffer);
		printf("curr len %d\n", curr_len);
	}
	
	//char* cookie_value = http_parse_header_field(buffer, strlen("Connection"), "Connection");
	parse_request(buffer, &request, len);
	printf("1");
	build_response(&request, &response);
	printf("2");
	print_response(&response);
	printf("3");
	send(socket,buffer, strlen(buffer), 0);
} 

void parse_request(char* buffer, request_info* request, int len){
	request->req_type = http_parse_method(buffer);
	request->command = parse_command(http_parse_uri(buffer));
	request->cacheControl = http_parse_header_field(buffer, len, "Cache-Control");
    	request->connection = http_parse_header_field(buffer, len, "Connection");
	request->host = http_parse_header_field(buffer, len, "Host");
	request->user_agent = http_parse_header_field(buffer, len, "User-Agent");
	request->content_length = http_parse_header_field(buffer, len, "Content-Length");
	request->parameters = http_parse_path(http_parse_uri(buffer));

}

command_type parse_command(char* uri){
	char* command_types[] = {"/login" , "/logout" , "/servertime", "/browser", "/redirect","/getfile","/putfile","/addcart","/delcart","/checkout", "/close"};
	command_type c;
	for (c =0; c<NOTA;c++)
		if (!strncasecmp(uri, command_types[c], strlen(command_types[c])))
			return c;
	return NOTA;
}

void build_response(request_info* request, response_info* response){
	switch(request->command){
		case LOGIN:
			break;
		case LOGOUT:
			break;
		case SERVERTIME:
			break;
		case BROWSER:
			break;
		case REDIRECT:
			break;
		case GET_FILE:
			break;
		case PUT_FILE:
			break;
		case ADD_CART:
			break;
		case DEL_CART:
			break;
		case CHECKOUT:
			break;
		case CLOSE:
			break;
		case NOTA:
			break;
	}
	response->info = request;
	response->date = "";
}

void print_response(response_info* response){
	printf("Connection: %s\n", response->info->connection);
    	printf("Cache-Control: '%s'\n", response->info->cacheControl);
	printf("Content-length: '%s'\n", response->info->content_length);
    	printf("Content-Type: '%s'\n", response->info->content_type);
    	printf("Date: '%s'\n", response->date);
}
