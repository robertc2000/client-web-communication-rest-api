#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <stdio.h>
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.c"

#define MAX_HEADER_SIZE 1000
#define MAX_LEN_SIZE 100
#define MAX_COOKIE_SIZE 200
#define LEN 50
#define MAX_IP_SIZE 16
#define HOST "34.118.48.238"
#define PORT 8080

char** compute_credentials(char* username, char* password) {
    char** credentials = (char**) malloc(1 * sizeof(char*));
    if (!credentials) {
        return NULL;
    }
    credentials[0] = (char*) calloc(MAX_LEN_SIZE, sizeof(char));
    if (!credentials[0]) {
        return NULL;
    }
    
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    json_object_set_string(root_object, "username", username);
    json_object_set_string(root_object, "password", password);
    credentials[0] = json_serialize_to_string_pretty(root_value);
    json_value_free(root_value);

    return credentials;
}

void make_register_request(char* host, char* username, char* password, int sockfd) {
    char** credentials = compute_credentials(username, password);
    if (!credentials) {
        return;
    }

    char* message = compute_post_request(host, "/api/v1/tema/auth/register", "application/json", NULL, 0, credentials, 1, NULL, 0);
    if (!message) {
    	return;
    }

    // free credentials
    free(credentials[0]);
    free(credentials);

    send_to_server(sockfd, message);
    char* response = receive_from_server(sockfd);

    free(message);
    
    if (strstr(response, "error")) {
    	printf("The username is already taken\n");
    } else {
    	printf("Register successful\n");
    }

    free(response);
}

char* get_session_cookie(char* response) {
	char* session_cookie = calloc(MAX_COOKIE_SIZE, sizeof(char));
    if (!session_cookie) {
    	return NULL;
    }

    char* start = strstr(response, "Set-Cookie: ");
    while (*(start - 1) != ' ') {
    	start++;
    }

    char* end = start;
    while (*end != ';') {
    	end++;
    }

    strncpy(session_cookie, start, end - start);
    return session_cookie;
}

char* make_login_request(char* host, char* username, char* password, int sockfd) {
    char** credentials = compute_credentials(username, password);
    if (!credentials) {
       	return NULL;
    }

    char* message = compute_post_request(host, "/api/v1/tema/auth/login", "application/json", NULL, 0, credentials, 1, NULL, 0);
    if (!message) {
    	return NULL;
    }

    send_to_server(sockfd, message);
    char* response = receive_from_server(sockfd);

    free(message);
    free(credentials);

    // check errors
    if (strstr(response, "No account with this username!")) {
    	printf("No account with this username!\n");
    	free(response);
    	return NULL;
    }

    if (strstr(response, "Credentials are not good!")) {
    	printf("Credentials are not good!\n");
    	free(response);
    	return NULL;
    }

    printf("Logged in as %s\n", username);

    // get session cookie
    char *session_cookie = get_session_cookie(response);
    free(response);
    return session_cookie;
}

void logout(char* host, char* session_cookie, int sockfd) {
	if (!session_cookie) {
		printf("Not logged in\n");
		return;
	}

	char** cookies = calloc(1, sizeof(char*));
	cookies[0] = session_cookie;

	char* message = compute_get_request(host, "/api/v1/tema/auth/logout", NULL, NULL, 0, cookies, 1);

    send_to_server(sockfd, message);
    char* response = receive_from_server(sockfd);
    free(message);
    free(cookies);

    if (strstr(response, "error")) {
    	printf("Error\n");
    	free(response);
    	return;
    }

    free(response);
    printf("Logout successful\n");
}

void reset_string (char** s) {
	if (s) {
		free(*s);
		*s = NULL;
	}
}

char* request_library_access(char* host, char* session_cookie, int sockfd) {
	if (session_cookie == NULL) {
		printf("Session cookie not valid\n");
		return NULL;
	}

	char** cookies = calloc(1, sizeof(char*));
	cookies[0] = session_cookie;

	char* message = compute_get_request(host, "/api/v1/tema/library/access", NULL, NULL, 0, cookies, 1);
	if (!message) {
		return NULL;
	}

	send_to_server(sockfd, message);
	char* response = receive_from_server(sockfd);
	free(message);
	free(cookies);
	
	if (strstr(response, "You are not logged in!")) {
		printf("You are not logged in!\n");
		free(response);
		return NULL;
	}

	char* json = strstr(response, "{\"");

	JSON_Value *val = json_parse_string(json);
	JSON_Object *object = json_value_get_object (val);
	free(response);

	char* token = (char*) json_object_dotget_string (object, "token");

	return token;
}

char** create_authorisation_header(char* jwt_token) {
	char** header = calloc(1, sizeof(char*));
	header[0] = calloc(MAX_HEADER_SIZE, sizeof(char));
	sprintf(header[0], "Authorization: Bearer %s", jwt_token);
	return header;
}

void get_books_info(char* host, char* jwt_token, char* session_cookie, int sockfd) {
	if (!jwt_token) {
		printf("JWT token is not valid\n");
		return;
	}

	if (!session_cookie) {
		printf("Session cookie not valid\n");
		return;
	}

	char** header = create_authorisation_header(jwt_token);
	char** cookies = calloc(1, sizeof(char*));
	cookies[0] = session_cookie;

	char* message = compute_get_request(host, "/api/v1/tema/library/books", NULL, header, 1, cookies, 1);
	send_to_server(sockfd, message);
	char* response = receive_from_server(sockfd);

	free(message);
	free(header);
	free(cookies);

	if (strstr(response, "Error when decoding tokenn!\n")) {
		printf("Error when decoding tokenn!\n");
		free(response);
		return;
	}

	char* json = strstr(response, "[{\"");

	// print books
	JSON_Array* array = json_value_get_array(json_parse_string(json));
	size_t len = json_array_get_count(array);

	for (int i = 0; i < len; i++) {
		JSON_Object* obj = json_array_get_object(array, i);
		double id = json_object_get_number(obj, "id");
		const char* title = json_object_get_string(obj, "title");
		printf("id = %d; title = %s\n", (int) id, title);
	}

	json_array_clear(array);
	free(response);
}

void get_book_info(char* host, char* jwt_token, char* session_cookie, int id, int sockfd) {
	if (!jwt_token) {
		printf("JWT token is not valid\n");
		return;
	}

	if (!session_cookie) {
		printf("Session cookie not valid\n");
		return;
	}

	char* url = calloc(MAX_LEN_SIZE, sizeof(char));
	sprintf(url, "/api/v1/tema/library/books/%d", id);

	char** header = create_authorisation_header(jwt_token);
	char** cookies = calloc(1, sizeof(char*));
	cookies[0] = session_cookie;

	char* message = compute_get_request(host, url, NULL, header, 1, cookies, 1);
	send_to_server(sockfd, message);
	char* response = receive_from_server(sockfd);

	free(message);
	free(url);
	free(header);
	free(cookies);

	if (strstr(response, "No book was found!")) {
		printf("No book was found!\n");
		free(response);
	 	return;
	}

	char* json = strstr(response, "[{\"");

	// print books
	JSON_Array* array = json_value_get_array(json_parse_string(json));
	size_t len = json_array_get_count(array);

	for (int i = 0; i < len; i++) {
		JSON_Object* obj = json_array_get_object(array, i);
		const char* title = json_object_get_string(obj, "title");
		const char* author = json_object_get_string(obj, "author");
		const char* genre = json_object_get_string(obj, "genre");
		const char* publisher = json_object_get_string(obj, "publisher");
		int page_count = (int) json_object_get_number(obj, "page_count");
		printf("title = %s\nauthor = %s\npublisher = %s\ngenre = %s\npage_count = %d\n\n", title, author, publisher, genre, page_count);
	}

	free(response);
}

char** create_book_json(char* title, char* author, char* genre, char* publisher, int page_count) {
	char** book = (char**) malloc(1 * sizeof(char*));

    book[0] = (char*) calloc(MAX_LEN_SIZE, sizeof(char));

    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);
    json_object_set_string(root_object, "title", title);
    json_object_set_string(root_object, "author", author);
    json_object_set_string(root_object, "genre", genre);
    json_object_set_number(root_object, "page_count", (double) (page_count));
    json_object_set_string(root_object, "publisher", publisher);
    book[0] = json_serialize_to_string_pretty(root_value);
    json_value_free(root_value);

    return book;
}

void add_book(char* host, char* jwt_token, char* session_cookie, char* title, char* author, char* genre, char* publisher, int page_count, int sockfd) {
	if (!jwt_token) {
		printf("JWT token is not valid\n");
		return;
	}

	if (!session_cookie) {
		printf("Session cookie not valid\n");
		return;
	}

	char** book = create_book_json(title, author, genre, publisher, page_count);

	char** header = create_authorisation_header(jwt_token);
	char** cookies = calloc(1, sizeof(char*));
	cookies[0] = session_cookie;


	char* message = compute_post_request(host, "/api/v1/tema/library/books", "application/json", header, 1, book, 1, cookies, 1);
    if (!message) {
    	return;
    }

    send_to_server(sockfd, message);
    char* response = receive_from_server(sockfd);

    free(message);
    free(book);
    free(header);
    free(cookies);

    if (strstr(response, "error")) {
    	printf("Error adding book\n");
    } else {
    	printf("Book added\n");
    }

    free(response);
}

void delete_book(char* host, char* jwt_token, char* session_cookie, int id, int sockfd) {
	if (!jwt_token) {
		printf("JWT token is not valid\n");
		return;
	}

	if (!session_cookie) {
		printf("Session cookie not valid\n");
		return;
	}

	char* url = calloc(MAX_LEN_SIZE, sizeof(char));
	sprintf(url, "/api/v1/tema/library/books/%d", id);

	char** header = create_authorisation_header(jwt_token);
	char** cookies = calloc(1, sizeof(char*));
	cookies[0] = session_cookie;

	char* message = compute_delete_request(host, url, header, 1, cookies, 1);
	send_to_server(sockfd, message);
	char* response = receive_from_server(sockfd);

	free(url);
	free(header);
	free(cookies);
	free(message);
	
	if (strstr(response, "No book was deleted!")) {
		printf("No book was deleted!\n");
	} else {
		printf("Book deleted\n");
	}

	free(response);
}

int main() {
    char host[MAX_IP_SIZE];
    strcpy(host, HOST);
    int port = PORT;
    int socket;
    char* session_cookie = NULL, *jwt_token = NULL;

    while (1) {
        char command[LEN];
        read_buffer(command);
        socket = open_connection(host, port, AF_INET, SOCK_STREAM, 0);

        // register
        if (!strcmp(command, "register")) {
            char username[LEN], password[LEN];
            read_buffer(username); read_buffer(password);
            make_register_request(host, username, password, socket);
            close(socket);
            continue;
        }

        // login
        if (!strcmp(command, "login")) {
            char username[LEN], password[LEN];
            read_buffer(username); read_buffer(password);
            reset_string(&session_cookie);
            session_cookie = make_login_request(host, username, password, socket);

            close(socket);
            continue;
        }

        // enter library
        if (!strcmp(command, "enter_library")) {
        	jwt_token = request_library_access(host, session_cookie, socket);
        	close(socket);
            continue;
        }

        // get book
        if (!strcmp(command, "get_book")) {
            int id; scanf("%d", &id);
            get_book_info(host, jwt_token, session_cookie, id, socket);
            close(socket);
            continue;
        }

        // get books
        if (!strcmp(command, "get_books")) {
        	get_books_info(host, jwt_token, session_cookie, socket);
        	close(socket);
            continue;
        }
    	
    	// add book
        if (!strcmp(command, "add_book")) {
            char title[LEN], author[LEN], genre[LEN], publisher[LEN];
            int page_count;
            read_buffer(title); read_buffer(author); read_buffer(genre);
            read_buffer(publisher);
            scanf("%d",  &page_count);

            add_book(host, jwt_token, session_cookie, title, author, genre, publisher, page_count, socket);
            close(socket);
            continue;
        }

        // delete book
        if (!strcmp(command, "delete_book")) {
            int id;
            scanf("%d", &id);
            delete_book(host, jwt_token, session_cookie, id, socket);
            close(socket);
            continue;
        }

        // logout
        if (!strcmp(command, "logout")) {
            logout(host, session_cookie, socket);
    		reset_string(&session_cookie);
    		reset_string(&jwt_token);
            close(socket);
            continue;
        }

        // exit
        if (!strcmp(command, "exit")) {
        	close(socket);
            break;
        }
    }

    return 0;
}