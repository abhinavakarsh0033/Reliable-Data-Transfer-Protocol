# =====================================
# Assignment 4 Final Submission
# Name: Abhinav Akarsh
# Roll number: 22CS30004
# makefile
# =====================================

all: init library user1 user2

library: libksocket.a

libksocket.a: k_socket.o
	ar rcs libksocket.a k_socket.o

k_socket.o: k_socket.c k_socket.h
	gcc -c k_socket.c -o k_socket.o

init: initksocket.c k_socket.h
	gcc -o init initksocket.c

user1: user1.c libksocket.a
	gcc -o user1 user1.c -L. -lksocket

user2: user2.c libksocket.a
	gcc -o user2 user2.c -L. -lksocket

clean:
	rm -f *.o *.a init user1 user2