/*
 * File: util.h
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdbool.h>

typedef enum {
    METHOD_GET, METHOD_POST, METHOD_HEAD, METHOD_OPTIONS, METHOD_PUT,
    METHOD_DELETE, METHOD_TRACE, METHOD_CONNECT, METHOD_UNKNOWN
} http_method;

extern const char *http_method_str[];

int http_header_complete(const char *request, int length);
http_method http_parse_method(const char *request);
char *http_parse_uri(char *request);
const char *http_parse_path(const char *uri);
char *http_parse_header_field(char *request, int length, const char *header_field);
const char *http_parse_body(const char *request, int length);
char *encode(const char *original, char *encoded);
char *decode(const char *original, char *decoded);

char* new_response_header(char* code, char* message);
void add_header_field(char** header, const char* name, const char* value);
void add_response_body(char** response, const char* body);
char* extract_parameter(const char* parameters, const char* name);
char* extract_cookie(const char* cookie, const char* name);
char* build_cookie_string(const char* name, const char* value, const char* expires, const char* path);
char* get_gm_time_string(time_t* raw_time);
char* get_local_time_string(time_t* raw_time);
char* itoa(int number);
char* hitoa(int number);
void append(char** original, char* addage);
#endif
