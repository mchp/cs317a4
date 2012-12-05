/*
 * File: service.h
 */

#ifndef _SERVICE_H_
#define _SERVICE_H_

#include "util.h"

typedef enum {
    LOGIN, LOGOUT, SERVERTIME, BROWSER,
    REDIRECT, GET_FILE, PUT_FILE, ADD_CART,
    DEL_CART, CHECKOUT, CLOSE, NOTA
} command_type;

typedef struct request_info{
	http_method req_type;
	command_type command;
	char* cache_control;
	char* connection;
	char* host;
	char* user_agent;
	char* content_length;
	char* content_type;
	char* transfer_encoding;
	char* cookie;
	char* if_modified_since;
	const char* parameters;
	const char* body;
} request_info;

typedef struct response_info{
	struct request_info* info;
	char* status_code;
	char* status_msg;
	char* content_type;
	char* connection;
	char* cache_control;
	char* content_length;
	char* transfer_encoding;
	char* location;
	char* last_modified;
	char* allow;
	char* set_cookie;
	char* body;
	char* more_cookies[12];
	int num_extra_cookies;
} response_info;

void handle_client(int socket);
void parse_request(char* buffer, request_info* request, int len);
command_type parse_command(char* uri);
void build_response(request_info* request, response_info* response);
char* print_response(response_info* response);
char* forbidden_command();
char* forbidden_checkout();

#endif
