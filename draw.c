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


void intToChars(int num, int chars) {
	int max = ((1 << (chars * 6)) - 1);

	if (num < 0) num = 0;
	if (num > max) num = max;
	
	int i;
	for (i = 0; i < chars; i++)
		' ' + (num >> i*6 & 63);
}

void fread2(FILE *file, unsigned char *buffer, size_t size) {
	size_t left = size;
	while (left) {
		size_t read = fread(buffer, 1, left, file);
		if (!read)
			error("failed to read body");
		left -= read;
		buffer += read;
	}
}

ssize_t httpRequest(struct sockaddr_in *addr, FILE **sockf_in, unsigned char**ret, char *format, ...) {
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
		content_size = 0;
		while (1) {
			// read chunked response
			read = getline(&line, &size, sockf); //chunk size
			size_t chunk_size = strtol(line, NULL, 16);
			if (!chunk_size)
				break;
			*ret = realloc(*ret, content_size + chunk_size);
			fread2(sockf, *ret + content_size, chunk_size);
			content_size += chunk_size;
			read = getline(&line, &size, sockf); //trailing newline
		}
		read = getline(&line, &size, sockf); //empty line after last chunk
	} else {
		if (content_size < 0)
			error("error: missing content size");
		*ret = realloc(*ret, content_size);
		fread2(sockf, *ret, content_size);
	}
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
	return content_size;
}

// send message in chat
bool postMessage(struct sockaddr_in *addr, FILE **sockf, char *room, char *name, char *text) {
	size_t length = strlen(name) + 2 + strlen(text) + 1;
	unsigned char *dummy = NULL;
	// todo: limit to 4096 message length
	httpRequest(
		addr, sockf, &dummy,
		"POST /stream/%s HTTP/1.1\r\nHost: oboy.smilebasicsource.com:80\r\nContent-Length: %ld\r\n\r\n(%c%c%s: %s\n",
		room, 3 + length,	'0' + (length & 63), '0' + (length >> 6 & 63), name, text
	);
	free(dummy);
}

bool isdata(unsigned char c) {
	return c>='0' && c<'0'+64;
}

bool isdatas(unsigned char *buf, int len) {
	while (len-->0) {
		if (!isdata(*buf++))
			return 0;
	}
	return 1;
}

int checkMessage(unsigned char *buf, unsigned char *end) {
	if (buf+2 >= end)
		return -1;
	if (*buf != '(')
		return -1;
	if (!isdatas(buf+1, 2))
		return -1;
	int len = buf[1]-'0' | (buf[2]-'0')<<6;
	if (len > end - (buf+3))
		return -1;
	return len;
}

int checkLine(unsigned char *buf, unsigned char *end) {
	if (buf+10 >= end)
		return -1;
	if (!isdatas(buf, 10))
		return -1;
	return 10;
}

int checkLines(unsigned char *buf, unsigned char *end) {
	if (buf+8 >= end) {
		return -1;
	}
	if (*buf != '.')
		return -1;
	if (!isdatas(buf+1, 6)) {
		return -1;
	}
	buf += 7;
	int len = 7;
	while (*buf != '.') {
		if (buf+4 >= end) {
			return -1;
		}
		if (!isdatas(buf, 4)) {
			return -1;
		}
		buf += 4;
		len += 4;
	}
	return len + 1;
}

// long polling recieve
bool pollPoll(struct sockaddr_in *addr, FILE **sockf, char *room, size_t *start) {
	unsigned char *buf = NULL;
	ssize_t len = httpRequest(
		addr, sockf, &buf,
		"GET /stream/%s?start=%ld HTTP/1.1\r\nHost: oboy.smilebasicsource.com:80\r\n\r\n",
		room, *start
	);
	bool hasDraw = 0;
	*start += len;
	unsigned char *end = buf + len;
	unsigned char *b = buf;
	while (buf < end) {
		int mlen;
		if ((mlen = checkMessage(buf, end)) != -1) {
			if (hasDraw)
				puts("[drawing]");
			fwrite(buf+3, 1, mlen, stdout);
			hasDraw = 0;
			buf += 3+mlen;
		} else if ((mlen = checkLines(buf, end)) != -1) {
			buf += mlen;
			hasDraw = 1;
		} else if ((mlen = checkLine(buf, end)) != -1) {
			buf += mlen;
			hasDraw = 1;
		} else {
			if (hasDraw)
				puts("[drawing]");
			hasDraw = 0;
			// invalid data!
			printf("\x1B[31m%c\x1B[m", *buf);
			buf++;
		}
	}
	if (hasDraw)
		puts("[drawing]");
	fflush(stdout);
	free(b);
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
