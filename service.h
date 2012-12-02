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

//don't think we need this anymore
typedef enum {
    NO_CACHE, PRIVATE, PUBLIC
} cache_control;

typedef enum {
    KEEP_ALIVE
} connection_type;

typedef struct request_info{
	http_method req_type; //do we really need this?
	command_type command;
	double http_version; //the util class doesn't have any funciton to extract this so that makes me think maybe we don't need it?
	char* cacheControl;
	char* connection;
	char* host;
	char* user_agent;
	char* content_length;
	char* content_type;
	const char* parameters;
} request_info;

typedef struct response_info{
	struct request_info* info;
	char* date;
	char* userID;
} response_info;

void handle_client(int socket);
void parse_request(char* buffer, request_info* request, int len);
command_type parse_command(char* uri);
void build_response(request_info* request, response_info* response);
void print_response(response_info* response);

#endif
