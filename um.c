// https://stackoverflow.com/a/22135885/6232794
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* fork */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */

void error(const char *msg) { fprintf(stderr, "%s\n", msg); exit(0); }

FILE *make_socket(struct sockaddr_in *serv_addr) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	if (connect(sockfd, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_in)) < 0)
		error("ERROR connecting");
	return fdopen(sockfd, "w+");
}

bool postMessage(struct sockaddr_in *addr, char *room, char *name, char *text) {
	static FILE *sockf = NULL;
	if (!sockf)
		sockf = make_socket(addr);
	size_t length = strlen(name) + 2 + strlen(text) + 1;
	fprintf(sockf, "POST /stream/%s HTTP/1.1\r\nHost: oboy.smilebasicsource.com:80\r\nContent-Length: %ld\r\n\r\n%s: %s\n", room, length, name, text);
	static char *line = NULL;
	static size_t size = 0;
	ssize_t read;

	read = getline(&line, &size, sockf); //response line 1
	if (read < 0) {
		perror("failed to read response");
		exit(1);
	}
	// read headers
	ssize_t content_size = -1;
	bool keep_alive = 0;
	while (1) {
		read = getline(&line, &size, sockf); //header
		if (read < 0)
			error("failed to read header");
		if (read == 2) // final line
			break;
		else { //process headers here
			//puts(line);
			if (!strncmp(line, "Content-Length: ", 14))
				content_size = strtol(line+14, NULL, 10);
			else if (!strncmp(line, "Connection: keep-alive\r\n", 24))
				keep_alive = 1;
		}
	}
	if (content_size == -1){
		error("error: missing content size");
	} else if (content_size) {
		unsigned char buffer[content_size];
		size_t left = content_size;	
		while (left) {
			size_t read = fread(buffer, 1, left, sockf);
			if (!read)
				error("failed to read body");
			left -= read;
		}
	}
	if (!keep_alive) {
		fclose(sockf);
		sockf = make_socket(addr);
	}
}

// this function exists specifically so we can allocate onto the stack
void read_chunk(FILE *file, size_t size) {
	unsigned char buffer[size];
	size_t left = size;
	while (left) {
		size_t read = fread(buffer, 1, left, file);
		if (!read)
			error("failed to read body");
		left -= read;
	}
	fwrite(buffer, 1, size, stdout); //print
}

// long polling recieve
bool pollPoll(struct sockaddr_in *addr, char *room, size_t *start) {
	static FILE *sockf = NULL;
	if (!sockf)
		sockf = make_socket(addr);
	fprintf(sockf, "GET /stream/%s?start=%ld HTTP/1.1\r\nHost: oboy.smilebasicsource.com:80\r\n\r\n", room, *start);
	static char *line = NULL;
	static size_t size = 0;
	ssize_t read;
	// {the program will spend a long time waiting here}
	read = getline(&line, &size, sockf); //response line 1
	// read headers
	bool keep_alive = 0;
	while (1) {
		read = getline(&line, &size, sockf); //header
		if (read == 2) // final line
			break;
		else {
			if (!strncmp(line, "Connection: keep-alive\r\n", 24))
				keep_alive = 1;
			//process headers here
		}
	}
	while (1) {
		// read chunked response
		read = getline(&line, &size, sockf); //chunk size
		size_t chunk_size = strtol(line, NULL, 16);
		if (!chunk_size)
			break;
		read_chunk(sockf, chunk_size);
		*start += chunk_size;
		read = getline(&line, &size, sockf); //trailing newline
	}
	read = getline(&line, &size, sockf); //empty line after last chunk
	if (!keep_alive) {
		fclose(sockf);
		sockf = make_socket(addr);
	}
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		error("usage: ./um room username");
	}
	
	// get server address
	struct hostent *server = gethostbyname("oboy.smilebasicsource.com");
	if (!server)
		error("ERROR, host not found");
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(80);
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	
	if (fork()) { // recv
		size_t start = 0;
		while (1) {
			pollPoll(&serv_addr, argv[1], &start);
		}
	} else { // send
		char *line = NULL;
		size_t size;
		while (1) {
			ssize_t r = getline(&line, &size, stdin);
			if (r > 0) {
				line[r-1] = '\0'; //strip newline
				postMessage(&serv_addr, argv[1], argv[2], line);
			}
		}
	}
}
// this file does not directly call free or fclose!
