// https://stackoverflow.com/a/22135885/6232794

#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit, atoi, malloc, free */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */

void error(const char *msg) { fprintf(stderr, "%s\n", msg); exit(0); }

struct hostent *server;
struct sockaddr_in serv_addr;

FILE *make_socket() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");
	return fdopen(sockfd, "w+");
}

FILE *postSocket;
void postMessage(char *room, char *name, char *text) {
	size_t length = strlen(name) + 2 + strlen(text) + 1;
	fprintf(postSocket, "POST /stream/%s HTTP/1.1\r\nHost: oboy.smilebasicsource.com\r\nContent-Length: %ld\r\n\r\n%s: %s\n", room, length, name, text);
	static char *line = NULL;
	size_t size = 0;
	ssize_t read;
	read = getline(&line, &size, postSocket); //response line 1
	fflush(postSocket); //rest of response doesn't matter lol
}

void read_chunk(FILE *file, size_t size) {
	unsigned char buffer[size];
	size_t left = size;
	while (left) {
		size_t read = fread(buffer, 1, left, file);
		if (read == 0) {
			error("failed to read body");
		}
		left -= read;
	}
	fwrite(buffer, 1, size, stdout);
}

FILE *pollSocket;
int start = 0;
void pollPoll(char *room) {
	fprintf(pollSocket, "GET /stream/%s?start=%d HTTP/1.1\r\nHost: oboy.smilebasicsource.com:80\r\n\r\n", room, start);
	static char *line = NULL;
	size_t size = 0;
	ssize_t read;
	read = getline(&line, &size, pollSocket); //response line 1
	// read headers
	while (1) {
		read = getline(&line, &size, pollSocket); //response line 1
		if (read == 2) // final line
			break;
		else
			; //process headers here
	}
	while (1) {
		// read chunked response
		read = getline(&line, &size, pollSocket); //chunk size
		size_t chunk_size = strtol(line, NULL, 16);
		if (!chunk_size)
			break;
		read_chunk(pollSocket, chunk_size);
		start += chunk_size;
		read = getline(&line, &size, pollSocket); //trailing newline
	}
	read = getline(&line, &size, pollSocket); //empty line after last chunk
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		error("usage: ./um room username");
	}

	// get server address
	server = gethostbyname("oboy.smilebasicsource.com");
	if (!server)
		error("ERROR, host not found");
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(80);
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	
	if (fork()) {
		// send
		postSocket = make_socket();
		char *line = NULL;
		size_t size;
		while (1) {
			ssize_t r = getline(&line, &size, stdin);
			if (r > 0) {
				line[r-1] = '\0'; //strip newline
				postMessage(argv[1], argv[2], line);
			}
		}
	} else {
		// recieve
		pollSocket = make_socket();
		while (1) 
			pollPoll(argv[1]);
	}
}
