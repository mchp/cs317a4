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


	if (!all_params_present(&request) || request.command == NOTA) {
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

char* user_logged_in(const char* username) {
	char* title = "Username: \0";
	int total_len = strlen(username)+strlen(title)+2;
	
	char* logged_in_str = (char*)malloc(total_len);
	strcpy(logged_in_str, title);
	strcpy(logged_in_str+strlen(title), username);

	logged_in_str[total_len - 2] = '\n';
	logged_in_str[total_len - 1] = '\0';

	return logged_in_str;
}

void set_content_length(response_info* response) {
	char* content_len_val = (char*)malloc(strlen(response->body));
	sprintf(content_len_val, "%d", (int)strlen(response->body));
	content_len_val = (char*)realloc(content_len_val, strlen(content_len_val));

	response->content_length = content_len_val;
}

void handle_login(request_info* request, response_info* response) {
	char* user_id = extract_parameter(request->parameters, "username");
	
	if (user_id) {
		char* max_age = "86400"; //24*60*60 i.e. 24 hours
		response->set_cookie = build_cookie_string("username", user_id, max_age, "", "/", 0);
		response->body = user_logged_in(user_id);
	} else {
		response->body = "Login failed\n";
	}
	
	response->cache_control = "no-cache";
	set_content_length(response);

	free(user_id);

}

void handle_logout(request_info* request, response_info* response) {
	char* user_id = extract_cookie(request->cookie, "username");
	printf("bye bye %s\n", user_id);
	if (user_id) {
		const char* pre = "User ";
		const char* post = " was logged out.\n";
		char* body = (char*)malloc(strlen(pre)+strlen(post)+strlen(user_id)+1);
		strcpy(body, pre);
		strcpy(body+strlen(pre), user_id);
		strcpy(body+strlen(pre)+strlen(user_id), post);

		response->body = body;
		response->set_cookie = build_cookie_string("username", user_id, "-1", "", "/", 0);
		
	} else {
		response->body = "Please login before logging out\n";
	}

	set_content_length(response);

	free(user_id);
}

void build_response(request_info* request, response_info* response){

	memset(response, 0, sizeof(response_info));
	response->info = request;

	//set some common fields that are true for most requests
	response->content_type = "text/plain";
	response->connection = "keep-alive";
	response->cache_control = "public";

	if (response->info->cache_control == NULL)//What is this? -mp
		response->info->cache_control = "public"; //TODO: check this is true

	switch(request->command){
		case LOGIN:
			handle_login(request, response);
			break;
		case LOGOUT:
			handle_logout(request, response);
			break;
		case SERVERTIME:
			break;
		//browser seems to be all done
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
			//TODO: return HTTP response code for Not Found here
			printf("Command not found\n");
			break;
	}
	if (response->info->content_type == NULL)
		response->info->content_type = "text/plain";
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


char* print_response(response_info* response){
	char* response_string = new_response_header("200", "OK");
	
	time_t raw_time;
	time(&raw_time);
	char* time_string = get_time_string(&raw_time);
	
	add_header_field(&response_string, "Date", time_string);

	add_header_field(&response_string, "Connection", response->connection);

	add_header_field(&response_string, "Cache-Control", response->cache_control);

	add_header_field(&response_string, "Content-Length", response->content_length);

	add_header_field(&response_string, "Content-Type", response->content_type);

	if (response->set_cookie) {
		add_header_field(&response_string, "Set-Cookie", response->set_cookie);
	}

	//TODO figure out when to use transfer-encoding	
	//add_header_field(&response_string, "Transfer-Encoding", response->info->transfer_encoding);

	add_response_body(&response_string, response->body);
	
	
	printf("%s\n", response_string);
	free(time_string);
	
	return response_string;
}
