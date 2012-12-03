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
	printf("body: %s\n", request->body);
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
	//printf("%s\n", request_string);
	parse_request(request_string, &request, len);

	if (!strncasecmp(request.parameters, "/favicon.ico", strlen("/favicon.ico"))) {
		return;
	}

	//print_request(&request);


	if (!all_params_present(&request)) {
		response_string = forbidden_command();
	} else if (request.command == NOTA){
		response_string = not_found_command();
	} else if (request.command == CHECKOUT && extract_cookie(request.cookie, "username") ==NULL){
		response_string = forbidden_checkout();
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
	request->body = http_parse_body(buffer, len);
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
	printf("welcome %s\n", user_id);
	if (user_id) {
		char* max_age = "86400"; //24*60*60 i.e. 24 hours
		response->set_cookie = build_cookie_string("username", user_id, max_age, "", "/", 0);
		response->body = user_logged_in(user_id);
		free(user_id);

	} else {
		response->body = "Login failed\n";
	}
	
	response->cache_control = "no-cache";
	set_content_length(response);

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

		free(user_id);		
	} else {
		response->body = "Please login before logging out\n";
	}

	set_content_length(response);

}

void handle_servertime(request_info* request, response_info* response) {

	time_t rawtime;
	time(&rawtime);

	char* time_string = get_local_time_string(&rawtime);
	
	char* user_id = extract_cookie(request->cookie, "username");

	char* body = time_string;
	if (user_id) {
		char* logged_in_str = user_logged_in(user_id);
		int logged_in_str_len = strlen(logged_in_str);
		int body_len = strlen(time_string)+logged_in_str_len+1;
		body = (char*)realloc(logged_in_str, body_len);

		strcpy(body+logged_in_str_len, time_string);
		body[body_len-1] = '\0';

		free(user_id);
		free(time_string);
	}

	response->body = body;
	set_content_length(response);
	response->cache_control = "no-cache";
}

void handle_browser(request_info* request, response_info* response){
	char* user_id = extract_cookie(request->cookie, "username");
	char* userAgent_str = "User-Agent: ";
	int userAgent_str_len = strlen(userAgent_str);	
	char* userAgent = request->user_agent;
	int userAgent_len = strlen(userAgent)+ userAgent_str_len +1;
	userAgent = (char*)realloc(userAgent_str, userAgent_len);
	strcpy(userAgent+userAgent_str_len, request->user_agent);
	userAgent[userAgent_len-1] = '\n';
	char* body = userAgent;	

	if (user_id) {
		char* logged_in_str = user_logged_in(user_id);
		int logged_in_str_len = strlen(logged_in_str);
		int body_len = strlen(userAgent)+logged_in_str_len+1;
		body = (char*)realloc(logged_in_str, body_len);

		strcpy(body+logged_in_str_len, userAgent);
		body[body_len-1] = '\0';

		free(user_id);
		free(userAgent);
	}

	response->body = body;
	set_content_length(response);
	response->cache_control = "private";
}

void handle_redirect (request_info* request, response_info* response){
	response->status_code = "303";
	response->status_msg = "See Other"; //???
	char* user_id = extract_cookie(request->cookie, "username");
	char* body = request->user_agent;
	if (user_id) {
		char* logged_in_str = user_logged_in(user_id);
		int logged_in_str_len = strlen(logged_in_str);
		int body_len = strlen(request->user_agent)+logged_in_str_len+1;
		body = (char*)realloc(logged_in_str, body_len);

		strcpy(body+logged_in_str_len, request->user_agent);
		body[body_len-1] = '\0';

		free(user_id);
	}

	response->body = body;
	set_content_length(response);
	response->cache_control = "private";
}

void handle_getfile(request_info* request, response_info* response){
	response->content_type = "application/octet-stream";
	//set response->last_modified
}

void handle_putfile(request_info* request, response_info* response){
	char* filename = extract_parameter(request->body, "filename");
	int filename_len = strlen(filename);
	FILE * fd;
	fd = fopen (filename,"w");
	if (fd != NULL) {
		char* content = extract_parameter(request->body, "content");
		fputs(content, fd);
		fclose(fd);
		free(content);

		char* save_success = " has been saved successfully.";
		filename = (char*)realloc(filename, filename_len+strlen(save_success));
		strcpy(filename+filename_len, save_success);
	
		response->body = filename;
		set_content_length(response);
	} else {
		response->status_code = "403";
		response->status_msg = "Forbidden";
		response->body = "HTTP 403, forbidden";
	}

	char* user_id = extract_cookie(request->cookie, "username");
	if (user_id) {
		char* logged_in_str = user_logged_in(user_id);
		int logged_in_str_len = strlen(logged_in_str);
		int body_len = strlen(response->body)+logged_in_str_len+1;
		user_id = (char*)realloc(logged_in_str, body_len);

		strcpy(user_id+logged_in_str_len, response->body);
		user_id[body_len-1] = '\0';

		free(response->body);
		response->body = user_id;
	}

	set_content_length(response);
	response->cache_control = "no-cache";
}

void handle_addcart(request_info* request, response_info* response){
}

void handle_delcart(request_info* request, response_info* response){
}

void handle_checkout(request_info* request, response_info* response){
	
}

void handle_close(request_info* request, response_info* response){
	response->connection = "close";
	response->body = "The connection will now be closed";
}

void build_response(request_info* request, response_info* response){

	memset(response, 0, sizeof(response_info));
	response->info = request;

	//set some common fields that are true for most requests
	response->content_type = "text/plain";
	response->connection = "keep-alive";
	response->cache_control = "public";
	response->status_code = "200";
	response->status_msg = "OK";

	switch(request->command){
		case LOGIN:
			handle_login(request, response);
			break;
		case LOGOUT:
			handle_logout(request, response);
			break;
		case SERVERTIME:
			handle_servertime(request, response);
			break;
		case BROWSER:
			handle_browser(request, response);
			break;
		case REDIRECT:
			response->location = extract_parameter(request->parameters, "url");
			break;
		case GET_FILE:
			handle_getfile(request, response);
			break;
		case PUT_FILE:
			handle_putfile(request, response);
			break;
		case ADD_CART:
			handle_addcart(request, response);
			break;
		case DEL_CART:
			handle_delcart(request, response);
			break;
		case CHECKOUT:
			handle_checkout(request, response);
			break;
		case CLOSE:
			handle_close(request, response);
			break;
		case NOTA:
			not_found_command();
			break;
	}
	if (response->info->content_type == NULL)
		response->info->content_type = "text/plain";
}

bool all_params_present(request_info* request){
	if ((request->command == BROWSER && request->user_agent == NULL) ||
	    (request->command == REDIRECT && extract_parameter(request->parameters, "url")==NULL))
		return false;
	return true;
}

char* not_found_command(){
	char* header = new_response_header("404", "Not Found");
	int header_len = strlen(header);
	char* statement = "Command not found\n"; 		
	char* body = statement;		
	int body_len = strlen(statement)+header_len+1;
	body = (char*)realloc(header, body_len);
	strcpy(body+header_len, statement);
	body[body_len-1] = '\0';
	return body;
}

char* forbidden_command(){
	char* header = new_response_header("403", "Forbidden");
	int header_len = strlen(header);
	char* statement = "Command forbidden\n"; //there is no explicit instruction that this should be the statement
	char* body = statement;		
	int body_len = strlen(statement)+header_len+1;
	body = (char*)realloc(header, body_len);
	strcpy(body+header_len, statement);
	body[body_len-1] = '\0';
	return body;
}

char* forbidden_checkout(){
	char* header = new_response_header("403", "Forbidden");
	int header_len = strlen(header);
	char* statement = "User must be logged in to checkout\n";
	char* body = statement;		
	int body_len = strlen(statement)+header_len+1;
	body = (char*)realloc(header, body_len);
	strcpy(body+header_len, statement);
	body[body_len-1] = '\0';
	return body;
}

char* print_response(response_info* response){
	char* response_string = new_response_header(response->status_code, response->status_msg);

	time_t raw_time;
	time(&raw_time);
	char* time_string = get_gm_time_string(&raw_time);
	
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

	if (response->info->command == REDIRECT)
		add_header_field(&response_string, "Location", response->location);
	else if (response->info->command == GET_FILE)
		add_header_field(&response_string, "Last-Modified", response->last_modified);

	add_response_body(&response_string, response->body);
	
	
	printf("%s\n", response_string);
	free(time_string);
	
	return response_string;
}
