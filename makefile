um: um.o
	cc -Wall -Wextra -pedantic um.o -o um -lpthread

um.o: um.c
	cc -c um.c -O2
