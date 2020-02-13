draw: draw.o
	cc -Wall -Wextra -pedantic draw.o -o draw -lpthread

draw.o: draw.c
	cc -c draw.c -O2
