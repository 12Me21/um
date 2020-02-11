// https://stackoverflow.com/a/22135885/6232794

#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit, atoi, malloc, free */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */

void error(const char *msg) { perror(msg); exit(0); }

struct hostent *server;
struct sockaddr_in serv_addr;

int make_socket() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");
	return sockfd;
}

int postSocket;
void postMessage(char *room, char *name, char *text) {
	postSocket = make_socket();
	size_t length = strlen(name) + 2 + strlen(text) + 1;
	dprintf(postSocket, "POST /stream/%s HTTP/1.0\r\nContent-Length: %ld\r\n\r\n%s: %s\n", room, length, name, text);
	
	char response[4096];
	int total, received, bytes;
	/* receive the response */
	total = sizeof(response)-1;
	received = 0;
	do {
		bytes = read(postSocket, response+received, total-received);
		if (bytes < 0)
			error("ERROR reading response from socket");
		if (bytes == 0)
			break;
		received+=bytes;
	} while (received < total);

	if (received == total)
		error("ERROR storing complete response from socket");

	//puts(response);
	
	close(postSocket);
}

int pollSocket;
int start = 0;
void pollPoll(char *room) {
	dprintf(pollSocket, "GET /stream/%s?start=%d HTTP/1.1\r\nHost: oboy.smilebasicsource.com:80\r\n\r\n", room, start);
	char buffer[4096] = {0};
	size_t bytes;
	// read header
	do {
		bytes = read(pollSocket, buffer, sizeof(buffer));
	} while (!bytes);
	// this should HOPEFULLY get the entire header in one pass
	/*char *len = strstr(buffer, "\r\nContent-Length: ");
	fwrite(buffer, 1, bytes, stdout);
	if (!len)
		error("no content length");
	int size = atoi(len + 18);
	printf("len: %d\n", size);*/
	char *end = strstr(buffer, "\r\n\r\n");
	if (!end)
		error("Couldn't find end of header");
	// now read the body
	end += 4;
	// read chunk size here
	// this assumes the first chunk has been recieved (which isn't always the case, maybe!)
	int size = strtol(end, NULL, 16);
	//printf("len: %d\n", size);
	end = strstr(end, "\r\n");
	end += 2;
	
	bytes = bytes - (end - buffer);
	if (bytes > size)
		bytes = size; //danger
	start += bytes;
	fwrite(end, 1, bytes, stdout);
	// keep reading...
	int readed = bytes;
	while (readed < size) {
		bytes = read(pollSocket, buffer, size - readed);
		if (bytes <= 0) {
			puts("oh no");
			break;
		}
		start += bytes;
		readed += bytes;
		//printf("%d/%d\n", readed, size);
		fwrite(buffer, 1, bytes, stdout);
	}
}

int main(int argc, char *argv[]) {
	// get server address
	server = gethostbyname("oboy.smilebasicsource.com");
	if (!server) error("ERROR, no such host");
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(80);
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	
	if (fork() == 0) {
		char *line = NULL;
		size_t size;
		while (1) {
			ssize_t r = getline(&line, &size, stdin);
			if (r > 0) {
				line[r-1] = '\0'; //strip newline
				postMessage(argv[1], "user", line);
			}
		}
	} else {
		pollSocket = make_socket();
		while(1){
			pollPoll(argv[1]);
		}
	}
}
