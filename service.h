/*
 * File: service.h
 */

#ifndef _SERVICE_H_
#define _SERVICE_H_

#include "util.h"
#include <stdbool.h>

typedef enum {
    LOGIN, LOGOUT, SERVERTIME, BROWSER,
    REDIRECT, GET_FILE, PUT_FILE, ADD_CART,
    DEL_CART, CHECKOUT, CLOSE, NOTA
} command_type;

typedef struct request_info{
	http_method req_type; //do we really need this?
	command_type command;
	char* cacheControl;
	char* connection;
	char* host;
	char* user_agent;
	char* content_length;
	char* content_type;
	char* transfer_encoding;
	const char* parameters;
} request_info;

typedef struct response_info{
	struct request_info* info;
	char* date;
	char* userID;
	char* last_modified;
} response_info;

void handle_client(int socket);
void parse_request(char* buffer, request_info* request, int len);
command_type parse_command(char* uri);
void build_response(request_info* request, response_info* response);
char* extract_parameter(char* param, char* string);
void print_response(response_info* response);
void forbidden_command();
bool all_params_present(request_info* request);

#endif
