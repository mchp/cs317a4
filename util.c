/*
 * File: util.c
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "util.h"

const char *http_method_str[] = {"GET", "POST", "HEAD", "OPTIONS", "PUT",
    "DELETE", "TRACE", "CONNECT"};

/*
 * If the HTTP header found in the first 'length' bytes of 'request'
 * is a complete HTTP header (i.e. contains an empty line indicating
 * end of header), returns the size of this header (in
 * bytes). Otherwise, returns -1. This function supports both requests
 * with LF only and with CR-LF. This function is only expected to be
 * called during the request retrieval process, and should not be used
 * after a parse function has been called, since parse functions
 * change the request in a way that changes the behaviour of this
 * function.
 */
int http_header_complete(const char *request, int length) {
    
    const char *body = http_parse_body(request, length);
    if (body && !memchr(request, '\0', body - request))
        return body - request;
    else
        return -1;
}

/*
 * Returns the method of the HTTP request. If the method is not one of
 * the RFC supported methods, returns METHOD_UNKNOWN.
 */
http_method http_parse_method(const char *request) {
    
    http_method m;
    
    // Ignore spaces in the beginning of the request
    while (isspace(*request)) request++;
    
    for(m = 0; m < METHOD_UNKNOWN; m++)
        if (!strncasecmp(request, http_method_str[m], strlen(http_method_str[m])) &&
            isspace(request[strlen(http_method_str[m])]))
            return m;
    return METHOD_UNKNOWN;
}

/*
 * Returns a pointer to the URI of the HTTP request. The original
 * request is changed with a new NULL byte, so you should not use the
 * request as a regular string after this call.
 */
char *http_parse_uri(char *request) {
    
    char *start, *end;
    
    // Ignore spaces in the beginning of the request
    while (isspace(*request)) request++;
    
    // Skip the method and spaces after that
    for (start = request; !isspace(*start); start++);
    for (               ;  isspace(*start); start++);
    
    // Find the next space
    for (end = start; *end && !isspace(*end); end++);
    
    // Change it to NULL so that URI is a standalone string.
    *end = '\0';
    
    return start;
}

/*
 * Returns a pointer to the path component of a URI.
 */
const char *http_parse_path(const char *uri) {
    
    if (*uri == '/') return uri;
    
    char *s = strchr(uri, ':');
    if (!s || strncmp(s, "://", 3)) return uri;
    
    for(s += 3; *s && *s != '/'; s++);
    return s;
}

/*
 * Returns a pointer to the value of a specific header field in the
 * HTTP request. The original request is changed with a new NULL byte,
 * so you should not use the request as a regular string after this
 * call.
 */
char *http_parse_header_field(char *request, int length, const char *header_field) {
    
    char *oldlf, *newlf, *newnull, *start;
    int len = strlen(header_field);
    
    // Ignore spaces in the beginning of the request
    while (isspace(*request) && length > 0) request++, length--;
    
    for (oldlf = memchr(request, '\n', length); oldlf; oldlf = newlf) {
        
        newlf = memchr(oldlf + 1, '\n', request + length - 1 - oldlf);
        
        // If there is a null byte, it might be there from a previous call
        newnull = memchr(oldlf + 1, '\0', request + length - 1 - oldlf);
        if (newnull && newnull < newlf) newlf = newnull;
        
        // If the request header ended, field was not found.
        if (newlf == oldlf + 1) return NULL;
        if (newlf == oldlf + 2 && oldlf[1] == '\r') return NULL;
        
        for(start = oldlf + 1; isspace(*start); start++);
        if (!strncasecmp(start, header_field, len) && start[len] == ':') {
            // Remove initial spaces
            for(start += len + 1; isspace(*start); start++);
            // Remove trailing spaces
            for(; isspace(*newlf); newlf--) *newlf = '\0';
            // Return what is left.
            return start;
        } else if (!*newlf) {
            // If header line ends with nulls already, moves on to the
            // last (corresponding to the old '\n').
            for(; !newlf[1]; newlf++);
        }
    }
    
    return NULL;
}

/*
 * Returns the position of the first byte of the body. If there is no
 * body, returns the position of the end of the header. Returns NULL
 * if the body is not within the informed length. This function
 * supports both requests with LF only and with CR-LF, and can be
 * called after calls to other parse functions.
 */
const char *http_parse_body(const char *request, int length) {
    
    const char *oldlf, *newlf, *newnull;
    
    // Ignore spaces in the beginning of the request
    while (isspace(*request) && length > 0) request++, length--;
    
    for (oldlf = memchr(request, '\n', length);
         oldlf && (newlf = memchr(oldlf + 1, '\n', request + length - 1 - oldlf));
         oldlf = newlf) {
        
        // If there is a null byte, it might be there from a previous call to header_field
        newnull = memchr(oldlf + 1, '\0', request + length - 1 - oldlf);
        if (newnull && newnull < newlf) newlf = newnull;
        
        if (newlf == oldlf + 1) return newlf + 1;
        if (newlf == oldlf + 2 && oldlf[1] == '\r') return newlf + 1;
        
        if (!*newlf) {
            // If header line ends with nulls already, moves on to the
            // last (corresponding to the old '\n').
            for(; !newlf[1]; newlf++);
        }
    }
    
    return NULL;
}

/*
 * Encodes the string 'original' into 'encoded'. It is recommended
 * that 'encoded' has space for at least 3*strlen(original)+1. For
 * convenience, returns 'encoded'.
 */
char *encode(const char *original, char *encoded) {
    
    char *e = encoded;
    for(;*original; original++, encoded++) {
        if (isalnum(*original))
            *encoded = *original;
        else if (*original == ' ')
            *encoded = '+';
        else {
            sprintf(encoded, "%%%02X", *original);
            encoded += 2;
        }
    }
    *encoded = '\0';
    return e;
}

/*
 * Decodes the string 'original' into 'decoded'. It is recommended
 * that 'decoded' has space for at least strlen(original)+1. For
 * convenience, returns 'decoded'.
 */
char *decode(const char *original, char *decoded) {
    
    char *d = decoded;
    for(;*original; original++, decoded++) {
        if (*original == '+')
            *decoded = ' ';
        else if (*original == '%') {
            unsigned int nr;
            if (!sscanf(original, "%%%02X", &nr)) break;
            *decoded = nr;
            original += 2;
        } else
            *decoded = *original;
    }
    *decoded = '\0';
    return d;
}

char* new_response_header(char* code, char* message) {
	const char* HTTP_RESPONSE = "HTTP/1.1 ";
	const int HTTP_RESPONSE_LEN = strlen(HTTP_RESPONSE);
	int msg_start = HTTP_RESPONSE_LEN + strlen(code) + 1;
	int header_len = msg_start+strlen(message)+2;

	char* new_response = (char*)malloc(header_len);

	strcpy(new_response, HTTP_RESPONSE);
	strcpy(new_response+HTTP_RESPONSE_LEN, code);
	strcpy(new_response+(msg_start-1), " ");
	strcpy(new_response+msg_start, message);
	new_response[header_len-2] = '\n';
	new_response[header_len-1] = '\0';

	return new_response;
}

void add_header_field(char** header, const char* name, const char* value) {
	int old_len = strlen(*header);
	int name_len = strlen(name);
	int val_len = strlen(value);	
	int new_len = old_len+name_len+val_len+4;
	*header = (char*)realloc(*header, new_len);

	strcpy((*header)+old_len, name);
	strcpy((*header)+old_len+name_len, ": ");
	strcpy((*header)+old_len+name_len+2, value);
	(*header)[new_len-2] = '\n';
	(*header)[new_len-1] = '\0';
}

void add_response_body(char** response, const char* body) {
	int old_len = strlen(*response);
	int body_len = strlen(body);
	int new_len = old_len+body_len+3;
	*response = (char*)realloc(*response, new_len);

	(*response)[old_len] = '\n';
	strcpy((*response)+old_len+1, body);
	(*response)[new_len-2] = '\n';
	(*response)[new_len-1] = '\0';
}

char* extract_parameter(const char* parameters, const char* name) {
	if (!parameters) {
		return NULL;
	}
	int total_len = strlen(parameters);
	int name_len = strlen(name);
	int val_len = 0;

	char* start_pos = memchr(parameters, '=', total_len);

	while (start_pos != NULL) {
		if (!strncasecmp(start_pos - name_len, name, name_len)) {
			break;
		}
		start_pos = memchr(start_pos+1, '=', total_len - (start_pos - parameters));
	}

	if (start_pos == NULL) {
		return NULL;
	}

	while (start_pos + val_len + 1 < parameters+total_len && start_pos[val_len+1] != '&'){
		val_len++;
	}
	char temp = start_pos[val_len+1];
	start_pos[val_len+1] = '\0';

	char* value = (char*)malloc(val_len+1);
	strcpy(value, start_pos+1);
	value[val_len] = '\0';

	start_pos[val_len+1] = temp;

	char* decoded_value = (char*)malloc(val_len+2);
	decode(value, decoded_value);
	free(value);
	return decoded_value;
}

int has_cookie(const char* cookie_string, const char* name) {
	if (!cookie_string) {
		return false;
	}
	int total_len = strlen(cookie_string);
	int name_len = strlen(name);
	char* start_pos = memchr(cookie_string, '=', total_len);

	while (start_pos != NULL) {
		if (!strncasecmp(start_pos - name_len, name, name_len)) {
			return true;
		}
		start_pos = memchr(start_pos+1, '=', total_len - (start_pos - cookie_string));
	}

	return false;
}

char* extract_cookie(const char* cookie_string, const char* name) {
	if (!cookie_string) {
		return NULL;
	}
	int total_len = strlen(cookie_string);

	if (total_len == 0) {
		return NULL;
	}
	int name_len = strlen(name);
	int val_len = 0;

	char* start_pos = memchr(cookie_string, '=', total_len);

	while (start_pos != NULL) {
		if (!strncasecmp(start_pos - name_len, name, name_len)) {
			break;
		}
		start_pos = memchr(start_pos+1, '=', total_len - (start_pos - cookie_string));
	}

	if (start_pos == NULL) {
		return NULL;
	}

	while (start_pos + val_len + 1 < cookie_string+total_len && start_pos[val_len+1] != ';'){
		val_len++;
	}

	char temp = start_pos[val_len+1];
	start_pos[val_len+1] = '\0';

	char* value = (char*)malloc(val_len+1);
	strcpy(value, start_pos+1);
	value[val_len] = '\0';

	start_pos[val_len+1] = temp;

	char* decoded_value = (char*)malloc(val_len+2);
	decode(value, decoded_value);
	free(value);
	return decoded_value;
}

void build_cookie_field(char* cookie_string, int* actual_len, const char* name, const char* value) {
	strcpy(cookie_string+*actual_len, name);
	*actual_len += strlen(name);

	strcpy(cookie_string+*actual_len, value);
	*actual_len += strlen(value);
}

char* build_cookie_string(const char* name, const char* value, const char* max_age, const char* path) {
	int max_len = 3*strlen(name)+3*strlen(value)+3*strlen(path)+ 40; //sorry about magic number everywhere; will fix
	int actual_len = 0;

	char* cookie_string = (char*)malloc(max_len);

	if (strlen(name) != 0) {
		char* encoded_name = (char*)malloc(strlen(name)*3+1);
		strcpy(cookie_string, encode(name, encoded_name));
		actual_len += strlen(encoded_name);
		cookie_string[actual_len++] = '=';

		free(encoded_name);
	}

	char* encoded_value = (char*)malloc(strlen(value)*3+1);
	strcpy(cookie_string+actual_len, encode(value, encoded_value));
	actual_len += strlen(encoded_value);
	free(encoded_value);

	if (strlen(max_age) != 0) {
		build_cookie_field(cookie_string, &actual_len, "; max-age=", max_age);
	}

	if (strlen(path) != 0) {
		build_cookie_field(cookie_string, &actual_len, "; path=", path);
	}

	cookie_string[actual_len] = '\0';

	cookie_string = (char*)realloc(cookie_string, actual_len+1);

	return cookie_string;
}


char* get_gm_time_string(time_t* raw_time) {
	struct tm* ptm;
	ptm = gmtime(raw_time);	

	int max_size = 1000;
	char* time_string = (char*)malloc(max_size);

	int final_size = strftime(time_string, max_size, "%a, %d %b %Y %T %Z", ptm);

	if (final_size < max_size) {
		time_string[final_size] = '\0';
	}

	time_string = (char*)realloc(time_string, final_size+1);
	return time_string;
}

char* get_local_time_string(time_t* raw_time) {
	struct tm* ptm;
	ptm = localtime(raw_time);	

	int max_size = 1000;
	char* time_string = (char*)malloc(max_size);

	int final_size = strftime(time_string, max_size, "%a, %d %b %Y %T %Z", ptm);

	if (final_size < max_size) {
		time_string[final_size] = '\0';
	}

	time_string = (char*)realloc(time_string, final_size+1);
	return time_string;
}

void append(char** original, char* addage) {
	int old_len = strlen(*original);
	int new_len = old_len+strlen(addage)+1;
	*original = (char*)realloc(*original, new_len);

	strcpy((*original)+old_len, addage);
	(*original)[new_len-1] = '\0';
}

char* itoa(int number) {
	char* str = (char*)malloc(number);
	sprintf(str, "%d", number);
	str = (char*)realloc(str, strlen(str));
	return str;
}

char* hitoa(int number) {
	char* str = (char*)malloc(number);
	sprintf(str, "%x", number);
	str = (char*)realloc(str, strlen(str));
	return str;
}
