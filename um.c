// https://stackoverflow.com/a/22135885/6232794
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* fork */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */

void error(const char *msg) { fprintf(stderr, "%s\n", msg); exit(0); }

void postMessage(FILE *sockf, char *room, char *name, char *text) {
	size_t length = strlen(name) + 2 + strlen(text) + 1;
	fprintf(sockf, "POST /stream/%s HTTP/1.1\r\nHost: oboy.smilebasicsource.com\r\nContent-Length: %ld\r\n\r\n%s: %s\n", room, length, name, text);
	fflush(sockf); // I really don't care about the response lol
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
void pollPoll(FILE *sockf, char *room, size_t *start) {
	fprintf(sockf, "GET /stream/%s?start=%ld HTTP/1.1\r\nHost: oboy.smilebasicsource.com:80\r\n\r\n", room, *start);
	static char *line = NULL;
	static size_t size = 0;
	ssize_t read;
	// {the program will spend a long time waiting here}
	read = getline(&line, &size, sockf); //response line 1
	// read headers
	while (1) {
		read = getline(&line, &size, sockf); //response line 1
		if (read == 2) // final line
			break;
		else
			; //process headers here
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
	
	int f = fork();
	// separate socket created in each process
	// sockets are only opened once, and reused for all requests
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");
	FILE *sockf = fdopen(sockfd, "w+");
	
	if (f) { // recv
		size_t start = 0;
		while (1) 
			pollPoll(sockf, argv[1], &start);
	} else { // send
		char *line = NULL;
		size_t size;
		while (1) {
			ssize_t r = getline(&line, &size, stdin);
			if (r > 0) {
				line[r-1] = '\0'; //strip newline
				postMessage(sockf, argv[1], argv[2], line);
			}
		}
	}
}
// this file does not directly call free or fclose!
