/*
 * File: service.c
 */

#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#include "service.h"

void handle_client(int socket) {
    
	int len = 10000;
	int curr_len = 0;
	char* request_string = (char*)malloc(len);
	char* response_string;

	request_info request;
	response_info response;

	while(http_header_complete(request_string, curr_len) == -1) {
		int bytes_received = recv(socket, request_string+curr_len, len-curr_len,0);
		if (bytes_received == 0) {
			free(request_string);
			return;
		}
		curr_len += bytes_received;

		if (curr_len >= len/2) {
			len *= 2;
			request_string = (char*)realloc(request_string, len);
		}
	}

	parse_request(request_string, &request, len);
	if (!strncasecmp(request.parameters, "/favicon.ico", strlen("/favicon.ico"))) {
		free(request_string);
		return;
	}

	
	if (request.content_length) {
		const char* body_so_far = http_parse_body(request_string, len);

		int body_len = atoi(request.content_length);
		curr_len = strlen(body_so_far);
		if (curr_len < body_len) {
			char* body = (char*)malloc(body_len);
			strcpy(body, body_so_far);

			while (curr_len < body_len) {
				int bytes_received = recv(socket, body+curr_len, body_len-curr_len,0);
	
				if (bytes_received <= 0) {
					free(request_string);
					return;
				}
	
				curr_len += bytes_received;
			}
			request.body = body;	
		} else {
			request.body = body_so_far;
		}
	}
	build_response(&request, &response);
	response_string = print_response(&response);

	int total_len = strlen(response_string);
	curr_len = 0;
	while (curr_len < total_len) {
		curr_len += send(socket,response_string+curr_len, total_len - curr_len, 0);	
	}

	free(request_string);
	free(response_string);
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
	request->if_modified_since = http_parse_header_field(buffer, len, "If-Modified-Since");
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

void prepend_user_to_body(request_info* request, response_info* response) {
	char* user_id = extract_cookie(request->cookie, "username");
	if (user_id) {
		if (response->body) {
			char* logged_in_str = user_logged_in(user_id);
			int logged_in_str_len = strlen(logged_in_str);
			int body_len = strlen(response->body)+logged_in_str_len+1;
			user_id = (char*)realloc(logged_in_str, body_len);

			strcpy(user_id+logged_in_str_len, response->body);
			user_id[body_len-1] = '\0';
		}
		response->body = user_id;
	}
}

void set_content_length(response_info* response) {
	if (response->body ==NULL)
		response->content_length = "0";
	else
		response->content_length = itoa(strlen(response->body));

}

void command_forbidden(response_info* response) {
	response->status_code = "403";
	response->status_msg = "Forbidden";
	response->body = "Command forbidden\n";
}

void handle_login(request_info* request, response_info* response) {
	char* user_id = extract_parameter(request->parameters, "username");	
	printf("welcome %s\n", user_id);
	if (user_id) {
		char* max_age = "86400"; //24*60*60 i.e. 24 hours
		response->set_cookie = build_cookie_string("username", user_id, max_age, "/");
		response->body = user_logged_in(user_id);
		free(user_id);
	} else {
		response->status_code = "403";
		response->status_msg = "Forbidden";
		response->body = "Login failed\n";
	}
	
	response->cache_control = "no-cache";
	set_content_length(response);

}

void handle_logout(request_info* request, response_info* response) {
	response->cache_control = "no-cache";
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
		response->set_cookie = build_cookie_string("username", user_id, "-1", "/");

		free(user_id);		
	} else {
		response->body = "Please login before logging out\n";
	}

	set_content_length(response);

}

void handle_servertime(request_info* request, response_info* response) {

	time_t rawtime;
	time(&rawtime);

	response->body = get_local_time_string(&rawtime);
	prepend_user_to_body(request, response);

	set_content_length(response);
	response->cache_control = "no-cache";
}

void handle_browser(request_info* request, response_info* response){	
	char* userAgent = request->user_agent;
	if (userAgent== NULL){
		command_forbidden(response);
	} else{
		response->body = request->user_agent;
	}
	prepend_user_to_body(request, response);
	set_content_length(response);
	response->cache_control = "private";
}

void handle_redirect (request_info* request, response_info* response){	
	response->status_code = "303";
	response->status_msg = "See Other"; 
	response->location = extract_parameter(request->parameters, "url");
	if (response->location == NULL){
		command_forbidden(response);
	}
	prepend_user_to_body(request, response);
	set_content_length(response);
}

void handle_getfile(request_info* request, response_info* response){
	char* filename_encoded = extract_parameter(request->parameters, "filename");
	if (!filename_encoded) {
		command_forbidden(response);

		prepend_user_to_body(request, response);
		set_content_length(response);
		return;
	}

	char* filename = decode(filename_encoded, (char*)malloc(strlen(filename_encoded)+1));
	struct stat filestatus;
    	stat(filename, &filestatus);
	
	response->last_modified = get_local_time_string(&filestatus.st_ctime);

	if (request->if_modified_since) {
		struct tm since_time;
		strptime(request->if_modified_since, "%a, %d %b %Y %T %Z", &since_time);


		if (filestatus.st_ctime <= mktime(&since_time)) {
			response->status_code = "304";
			response->status_msg = "Not Modified";
			response->body = "HTTP 304 Not Modified";

			prepend_user_to_body(request, response);
			set_content_length(response);

			return;
		} 
	}

	FILE* fd;
	fd = fopen(filename, "r");

	if (fd != NULL) {
		response->transfer_encoding = "chunked";
		response->content_type = "application/octet-stream";
		int buffer_size = 100;
		char buffer[buffer_size];

		int read_bytes = fread(buffer, sizeof(char), buffer_size-3, fd);
		response->body = (char*)malloc(1);
		response->body[0] = '\0';
		while (read_bytes) {
			append(&(response->body), hitoa(read_bytes));
			append(&(response->body), "\r\n");
			buffer[read_bytes++]='\r';
			buffer[read_bytes++] = '\n';
			buffer[read_bytes] ='\0';
			append(&(response->body), buffer);
			read_bytes = fread(buffer, sizeof(char), buffer_size-3, fd);
		}

		append(&(response->body), "0\r\n\r\n");
		
	} else {
		response->status_code = "404";
		response->status_msg = "Not Found";
		response->body = "HTTP 404, not found";
		prepend_user_to_body(request, response);
		set_content_length(response);
	}
}

void handle_putfile(request_info* request, response_info* response){

	response->cache_control = "no-cache";
	char* filename = extract_parameter(request->body, "filename");

	if (!filename) {
		command_forbidden(response);

		prepend_user_to_body(request, response);
		set_content_length(response);
		return;
	}

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
	prepend_user_to_body(request, response);
	set_content_length(response);

}

char* get_free_item(request_info* request){
	char* item_cookies[] = {"item1","item2","item3","item4","item5","item6","item7","item8","item9","item10","item11","item12"};
	int max_num_cookies = 12;
	int i;
	for (i=0; i<max_num_cookies; i++) {
		if (!has_cookie(request->cookie, item_cookies[i])) {
			return item_cookies[i];
		}
	}

	return NULL;
}

int get_free_item_num(request_info* request){
	char* item_cookies[] = {"item1","item2","item3","item4","item5","item6","item7","item8","item9","item10","item11","item12"};
	int max_num_cookies = 12;
	int i;
	for (i=0; i<max_num_cookies; i++) {
		if (!has_cookie(request->cookie, item_cookies[i])) {
			return i;
		}
	}
	return -1;
}

void append_cookie_list_item(char* list, int* offset, int pos, char* item) {
	int off = *offset;

	strcpy(list+off, itoa(pos));
	off += strlen(itoa(pos));
	strcpy(list+off, ". ");
	off += 2;
	strcpy(list+off, item);
	off += strlen(item);
	list[off] = '\n';
	off++;

	*offset = off;
}

char* get_cookie_list(request_info* request, char* item, int del_index){
	char* item_cookies[] = {"item1","item2","item3","item4","item5","item6","item7","item8","item9","item10","item11","item12"};	
	char* cookie_list= (char*)malloc(12*(64+3)+1);
	int offset = 0;

	char* curr_cookie = extract_cookie(request->cookie, item_cookies[0]);
	int i = 1;
	while(curr_cookie) {
		if (i<del_index) {
			append_cookie_list_item(cookie_list, &offset, i, curr_cookie);
		} else if (i>del_index){
			append_cookie_list_item(cookie_list, &offset, i-1, curr_cookie);
		}

		free(curr_cookie);
		curr_cookie = extract_cookie(request->cookie, item_cookies[i++]);
	}

	if (item) {
		append_cookie_list_item(cookie_list, &offset, i, item);
	}
	cookie_list[offset] = '\0';
	return cookie_list;
}

void handle_addcart(request_info* request, response_info* response){
	response->cache_control = "no-cache";
	char* item = extract_parameter(request->parameters, "item");
	if (item == NULL){
		command_forbidden(response);
	} else{
		char* item_num= get_free_item(request);	
		if (item_num){
			response->set_cookie = build_cookie_string(item_num, item, "86400", "/");		
			response->body = get_cookie_list(request, item, 14);		
		} else {
			response->body = "Cart full. Please proceed to checkout.";
		}
	}
	prepend_user_to_body(request, response);
	set_content_length(response);
}

void handle_delcart(request_info* request, response_info* response){
	response->cache_control = "no-cache";
	char* item_names[] = {"item1","item2","item3","item4","item5","item6","item7","item8","item9","item10","item11","item12"};	
	char* item = extract_parameter(request->parameters, "itemnr");

	if (item == NULL){
		command_forbidden(response);
	} else{
		int del_item_num= atoi(item);
		response->body = get_cookie_list(request, NULL, del_item_num);

		char* curr_item = extract_cookie(request->cookie, item_names[del_item_num]);
		int extra = 0;
		while (curr_item) {
			response->more_cookies[extra] = build_cookie_string(item_names[del_item_num-1], curr_item, "86400", "/");
			free(curr_item);
			del_item_num++;
			extra++;
			curr_item = extract_cookie(request->cookie, item_names[del_item_num]);
		}
		response->num_extra_cookies = extra;

		response->set_cookie = build_cookie_string(item_names[del_item_num-1], "", "-1", "/");					
	}
	prepend_user_to_body(request, response);
	set_content_length(response);
}

void handle_checkout(request_info* request, response_info* response){
	response->cache_control = "no-cache";
	char* item_cookies[] = {"item1","item2","item3","item4","item5","item6","item7","item8","item9","item10","item11","item12"};	
	if (extract_cookie(request->cookie, "username") ==NULL){
		response->status_code = "403";
		response->status_msg = "Forbidden";
		response->body = "User must be logged in to checkout\n";
	} else{
		response->body = get_cookie_list(request, NULL, -1);
		//make a file or append a file
		FILE * fd;
		char* filename = "CHECKOUT.txt";
		fd = fopen (filename,"a");
		fputs(get_cookie_list(request, NULL, -1), fd);
		fclose(fd);

		//delete all item cookies
		int total_items = get_free_item_num(request);
		int i;
		char* cookie_string;
		if (total_items == 0){
			set_content_length(response);
			return;
		}	
		cookie_string = build_cookie_string(item_cookies[0], extract_cookie(request->cookie, item_cookies[0]), "-1", "/");
		//char* temp;
		//int temp_len;
		//int curr_len;
		for (i=0; i<total_items; i++){
			response->more_cookies[i] = build_cookie_string(item_cookies[i], extract_cookie(request->cookie, item_cookies[i]), "-1", "/");
			/*temp_len = strlen(temp);
			curr_len = strlen(cookie_string);
			cookie_string = (char*) realloc(cookie_string, temp_len+curr_len+1);
			strcpy(cookie_string+curr_len, temp);
			free(temp);*/		
		}
		response->num_extra_cookies = total_items;		
		response->set_cookie = cookie_string;
		prepend_user_to_body(request, response);		
	}
	set_content_length(response);
}

void handle_close(request_info* request, response_info* response){
	response->connection = "close";
	response->body = "The connection will now be closed";
	prepend_user_to_body(request, response);
	set_content_length(response);
	response->cache_control = "private";
}


void not_found_command(request_info* request, response_info* response){
	response->status_code = "404";
	response->status_msg = "Not Found";
	response->body = "Command not found\n";
	prepend_user_to_body(request, response);
	set_content_length(response);
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
			handle_redirect(request, response);
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
		default:
			not_found_command(request, response);
	}
}

char* print_response(response_info* response){
	char* response_string = new_response_header(response->status_code, response->status_msg);

	time_t raw_time;
	time(&raw_time);
	char* time_string = get_gm_time_string(&raw_time);
	add_header_field(&response_string, "Date", time_string);

	add_header_field(&response_string, "Connection", response->connection);

	add_header_field(&response_string, "Cache-Control", response->cache_control);

	if (response->content_length) {
		add_header_field(&response_string, "Content-Length", response->content_length);
	} else {
		add_header_field(&response_string, "Transfer-Encoding", response->transfer_encoding);
	}

	add_header_field(&response_string, "Content-Type", response->content_type);

	if (response->set_cookie) {
		add_header_field(&response_string, "Set-Cookie", response->set_cookie);
	}
	if (response->num_extra_cookies >0){
		int i;		
		for(i= 0; i<response->num_extra_cookies; i++){
			add_header_field(&response_string, "Set-Cookie", response->more_cookies[i]);	
		}
	}

	if (response->location) {
		add_header_field(&response_string, "Location", response->location);
	}

	if (response->last_modified){
		add_header_field(&response_string, "Last-Modified", response->last_modified);
	}

	if (response->body)
		add_response_body(&response_string, response->body);
	
	printf("%s\n", response_string);
	free(time_string);
	
	return response_string;
}
