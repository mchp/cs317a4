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
	build_response(&request, &response);
	if (!all_params_present(&request))
		forbidden_command();
	else if (request.command != NOTA)
		print_response(&response);
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
	request->transfer_encoding = http_parse_header_field(buffer, len, "Transfer-Encoding");
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
	char* location= "";
	response->info = request;
	response->userID = NULL;
	response->last_modified = NULL;
	if (response->info->cacheControl == NULL)
		response->info->cacheControl = "public"; //TODO: check this is true
	switch(request->command){
		case LOGIN:
			break;
		case LOGOUT:
			break;
		case SERVERTIME:
			response->info->cacheControl = "no-cache";
			break;
		//browser seems to be all done
		case BROWSER:
			response->info->cacheControl = "private";
			printf("User-Agent: %s\n", response->info->user_agent);
			break;
		case REDIRECT:
			//TODO:return HTTP with the code for "See Other" (I don't know what that means yet)
			printf("Location: %s\n", extract_parameter("url=", decode(response->info->parameters, location)));
			break;
		case GET_FILE:
			//if success
			response->info->content_type = "application/octet-stream";
			//set response->last_modified;
			break;
		case PUT_FILE:
			response->info->cacheControl = "no-cache";
			break;
		case ADD_CART:
			break;
		case DEL_CART:
			break;
		case CHECKOUT:
			if (response->userID == NULL)
				printf("User must be logged in to checkout."); 
			break;
		case CLOSE:
			response->info->connection = "close";
			printf("The connection will now be closed.");
			break;
		case NOTA:
			//TODO: return HTTP response code for Not Found here
			printf("Command not found");
			break;
	}
	if (response->info->content_type == NULL)
		response->info->content_type = "text/plain";
	response->date = "";
}

//TODO: implement this
char* extract_parameter(char* param, char* string){
	//find param in string	
	//return the string that follows until you find '&' or ' '
	return "";
}

bool all_params_present(request_info* request){
	char* location = "";	
	if ((request->command == BROWSER && request->user_agent == NULL) ||
	    (request->command == REDIRECT && extract_parameter("url=", decode(request->parameters,location))))
		return false;
	return true;
}

void forbidden_command(){
	//TODO: return HTTP response code for Forbidden here
	printf("Command not found");
}

void print_response(response_info* response){
	printf("Connection: %s\n", response->info->connection);
    	printf("Cache-Control: '%s'\n", response->info->cacheControl);
	//TODO: find out which one of the following two to display when	
	printf("Content-length: '%s'\n", response->info->content_length);
	printf("Transfer-Encoding: '%s'\n", response->info->transfer_encoding);
    	printf("Content-Type: '%s'\n", response->info->content_type);
    	printf("Date: '%s'\n", response->date);
	if (response->info->command == GET_FILE)
		printf("Last-Modified: '%s'\n", response->last_modified);
}
