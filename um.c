#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>

// https://stackoverflow.com/a/22135885/6232794

void error(const char *msg) { fprintf(stderr, "%s\n", msg); exit(0); }

FILE *make_socket(struct sockaddr_in *serv_addr) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	if (connect(sockfd, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_in)) < 0)
		perror("ERROR connecting");
	return fdopen(sockfd, "w+");
}

// read a chunk and print it (if start is not NULL)
// increment *start by the size of the data
void read_chunk(FILE *file, ssize_t size, void *start) {
	if (size < 0)
		return;
	unsigned char buffer[size];
	size_t left = size;
	while (left) {
		size_t read = fread(buffer + (size - left), 1, left, file);
		if (!read)
			error("failed to read body");
		left -= read;
	}
	if (start) {
		*(size_t *)start += size;
		fwrite(buffer, 1, size, stdout); //print
	}
}

// the callback function MUST read the requested amount of data from the file
// callback will be called 1 extra time after all data is read, with size set to -1
void httpRequest(struct sockaddr_in *addr, FILE **sockf_in, void (*callback)(FILE *, ssize_t, void *), void *user, char *format, ...) {
	FILE *sockf;
	if (!sockf_in || !*sockf_in)
		sockf = make_socket(addr);
	else
		sockf = *sockf_in;
	va_list args;
	va_start(args, format);
	vfprintf(sockf, format, args);
	va_end(args);
	
	char *line = NULL;
	size_t size = 0;
	ssize_t read;
	// the program might wait here for a while
	read = getline(&line, &size, sockf); //response line 1
	if (read < 0) {
		perror("failed to read response");
		exit(1);
	}
	// read headers
	ssize_t content_size = -1;
	bool keep_alive = 0;
	bool chunked = 0;
	while (1) {
		read = getline(&line, &size, sockf); //header
		if (read < 0)
			error("failed to read header");
		if (read == 2) // final line
			break;
		else { //process headers here
			if (!strncmp(line, "Content-Length: ", 14))
				content_size = strtol(line+14, NULL, 10);
			else if (!strcmp(line, "Connection: keep-alive\r\n"))
				keep_alive = 1;
			else if (!strcmp(line, "Transfer-Encoding: chunked\r\n"))
				chunked = 1;
		}
	}
	if (chunked) {
		while (1) {
			// read chunked response
			read = getline(&line, &size, sockf); //chunk size
			size_t chunk_size = strtol(line, NULL, 16);
			if (!chunk_size)
				break;
			callback(sockf, chunk_size, user);
			read = getline(&line, &size, sockf); //trailing newline
		}
		read = getline(&line, &size, sockf); //empty line after last chunk
	} else {
		if (content_size < 0)
			error("error: missing content size");
		callback(sockf, content_size, user);
	}
	callback(sockf, -1, user);
	free(line);
	if (sockf_in) {
		if (!keep_alive) {
			fclose(sockf);
			sockf = make_socket(addr);
		}
		*sockf_in = sockf;
	} else {
		fclose(sockf);
	}
}

// send message in chat
bool postMessage(struct sockaddr_in *addr, FILE **sockf, char *room, char *name, char *text) {
	size_t length = strlen(name) + 2 + strlen(text) + 1;
	httpRequest(addr, sockf, read_chunk, NULL, "POST /stream/%s HTTP/1.1\r\nHost: oboy.smilebasicsource.com:80\r\nContent-Length: %ld\r\n\r\n%s: %s\n", room, length, name, text);
}

// long polling recieve
bool pollPoll(struct sockaddr_in *addr, FILE **sockf, char *room, size_t *start) {
	httpRequest(addr, sockf, read_chunk, start, "GET /stream/%s?start=%ld HTTP/1.1\r\nHost: oboy.smilebasicsource.com:80\r\n\r\n", room, *start);
}

struct data {
	struct sockaddr_in addr;
	char *username;
	char *room;
	size_t start;
	pthread_t thread2;
	FILE *sockf2;
};

void *thread2(void *data_in) {
	struct data *data = data_in;
	while (1) {
		pollPoll(&data->addr, &data->sockf2, data->room, &data->start);
	}
}

void switch_room(struct data *data, char *room) {
	printf(" Switching to room: '%s'\n", room);
	// cancel request
	pthread_cancel(data->thread2);
	data->sockf2 = NULL;
	data->start = 0;
	free(data->room);
	data->room = strdup(room);
	// start new request
	pthread_create(&data->thread2, NULL, thread2, data);
	//todo: cancel other poll
}

void *thread1(void *data_in) {
	struct data *data = data_in;
	char *line = NULL;
	size_t size;
	FILE *sockf;
	while (1) {
		ssize_t r = getline(&line, &size, stdin);
		if (r > 0) {
			line[r-1] = '\0'; //strip newline
			if (line[0]=='/') {
				switch_room(data, line+1);
			} else {
				postMessage(&data->addr, &sockf, data->room, data->username, line);
			}
		}
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
	
	struct data data;
	data.addr.sin_family = AF_INET;
	data.addr.sin_port = htons(80);
	memcpy(&data.addr.sin_addr.s_addr, server->h_addr, server->h_length);
	data.username = strdup(argv[2]);
	data.room = strdup(argv[1]);
	data.start = 0;
	data.sockf2 = NULL;
	
	pthread_t t1;
	pthread_create(&t1, NULL, thread1, &data);
	pthread_create(&data.thread2, NULL, thread2, &data);
	
	pthread_join(t1, NULL);
	pthread_join(data.thread2, NULL);
}
