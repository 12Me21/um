um: um.o
	cc -Wall -Wextra -pedantic um.o -o um

um.o:
	cc -c um.c -O2
