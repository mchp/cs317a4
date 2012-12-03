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
#include <time.h>


#include "service.h"

void print_request(request_info* request) {
	printf("------ Request Info --------\n");
	printf("req_type: %d\n", request->req_type);
	printf("command: %d\n", request->command);
	printf("cache_control: %s\n", request->cache_control);
	printf("connection: %s\n", request->connection);
	printf("host: %s\n", request->host);
	printf("user_agent: %s\n", request->user_agent);
	printf("content_length: %s\n", request->content_length);
	printf("cookie:%s\n", request->cookie);
	printf("transfer_encoding:%s\n", request->transfer_encoding);
	printf("parameters:%s\n", request->parameters);
	printf("---- End of Request Info ------\n");
}

void handle_client(int socket) {
    
	int len = 10000;
	int curr_len = 0;
	char* request_string = malloc(len);
	char* response_string;

	request_info request;
	response_info response;
	while(http_header_complete(request_string, curr_len) == -1) {
		curr_len += recv(socket, request_string+curr_len, len-curr_len,0);	
	}

	parse_request(request_string, &request, len);

	if (!strncasecmp(request.parameters, "/favicon.ico", strlen("/favicon.ico"))) {
		return;
	}

	print_request(&request);

	if (!all_params_present(&request) || request.command != NOTA) {
		response_string = forbidden_command();
	} else {
		build_response(&request, &response);
		response_string = print_response(&response);
	}

	send(socket,response_string, strlen(response_string), 0);
} 

void parse_request(char* buffer, request_info* request, int len){
	request->req_type = http_parse_method(buffer);
	request->command = parse_command(http_parse_uri(buffer));
	request->cache_control = http_parse_header_field(buffer, len, "Cache-Control");
    	request->connection = http_parse_header_field(buffer, len, "Connection");
	request->host = http_parse_header_field(buffer, len, "Host");
	request->user_agent = http_parse_header_field(buffer, len, "User-Agent");
	request->content_length = http_parse_header_field(buffer, len, "Content-Length");
	request->transfer_encoding = http_parse_header_field(buffer, len, "Transfer-Encoding");
	request->cookie = http_parse_header_field(buffer, len, "Cookie");
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
	if (response->info->cache_control == NULL)
		response->info->cache_control = "public"; //TODO: check this is true
	switch(request->command){
		case LOGIN:
			break;
		case LOGOUT:
			break;
		case SERVERTIME:
			response->info->cache_control = "no-cache";
			break;
		//browser seems to be all done
		case BROWSER:
			response->info->cache_control = "private";
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
			response->info->cache_control = "no-cache";
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
			printf("Command not found\n");
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

char* forbidden_command(){
	//TODO: return HTTP response code for Forbidden here
	printf("Command not found\n");
	return "Command not found\n";
}

char* get_time_string() {

	time_t raw_time;
	struct tm* ptm;
	time(&raw_time);
	ptm = gmtime(&raw_time);	

	int max_size = 1000;
	char* time_string = (char*)malloc(max_size*sizeof(char));

	int final_size = strftime(time_string, max_size, "%a, %d %b %Y %T %Z", ptm);

	if (final_size < max_size) {
		time_string[final_size] = '\0';
	}

	return time_string;
}

char* print_response(response_info* response){
	char* response_string = new_response_header("200", "OK");

	char* time_string = get_time_string();
	
	add_header_field(&response_string, "Date", time_string);
	add_header_field(&response_string, "Connection", response->info->connection);

	add_header_field(&response_string, "Cache-Control", response->info->cache_control);
	add_header_field(&response_string, "Content-length", "0");

	//TODO figure out when to use transfer-encoding	
	//add_header_field(&response_string, "Transfer-Encoding", response->info->transfer_encoding);

	if (response->info->command == GET_FILE) {
		add_header_field(&response_string, "Last-Modified", response->last_modified);
		add_header_field(&response_string, "Content-Type", "application/octet-stream\0");
	} else {
		add_header_field(&response_string, "Content-Type", "text/plain");
	}
	
	printf("%s\n", response_string);
	free(time_string);
	
	return response_string;
}
