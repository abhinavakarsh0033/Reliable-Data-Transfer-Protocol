/*=====================================
Assignment 4 Final Submission
Name: Abhinav Akarsh
Roll number: 22CS30004
k_socket.h
=====================================*/

#ifndef K_SOCKET_H
#define K_SOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#define T 5     // timeout in seconds
#define DROP_PROB 0.05  // probability of dropping a packet

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)
#define MAX_WND 10
#define BUFLEN 512
#define MAX_SOCKETS 10
#define SOCK_KTP 1234
#define MAX_SEQ 256

// error codes
#define SUCCESS 0
#define ENOSPACE 1001
#define ENOTBOUND 1002
#define ENOMESSAGE 1003
#define EINVALIDTYPE 1004
#define EINVALIDSOCKET 1005

// socket states
#define FREE 0
#define TO_CREATE_SOCKET 1
#define SOCKET_CREATED 2
#define TO_BIND 3
#define BOUND 4
#define TO_CLOSE 5

// flag for rwnd
#define NOSPACE 2112
#define NORMAL 1221

typedef struct _buffer {
    char data[MAX_WND][BUFLEN]; // buffer
    int len[MAX_WND];           // length of each message in the buffer
    int read_end;       // read end pointer of the buffer
    int write_end;      // write end pointer of the buffer
    int count;          // number of messages in the buffer (yet to be acknowledged or read)
} buffer;

typedef struct _window {
    int marked[MAX_SEQ];    // marked sequence numbers, 1 represents acknowledged, 0 represents not acknowledged (only for rwnd)
    int base;               // base pointer
    int size;               // size of the window
    int count;              // number of messages in the window yet to be acknowledged (only for swnd)
} window;

typedef struct _ktp_socket {
    int state;          // state: one of FREE, TO_CREATE_SOCKET, SOCKET_CREATED, TO_BIND, BOUND, TO_CLOSE
    int pid;            // Process ID of the process that created the socket
    int sockfd;         // Socket file descriptor
    window swnd;        // Send window
    window rwnd;        // Receive window
    int flag;                       // flag for rwnd when buffer is full
    struct sockaddr_in src_addr;    // Source address
    struct sockaddr_in dst_addr;    // Destination address
    buffer send_buf;                // Send buffer
    buffer recv_buf;                // Receive buffer
    long long int last_sent;        // Last time data was sent
    int trans_cnt;                  // Number of times data packet has been sent
    int msg_count;                  // Number of messages sent
    int error;                      // Error code
} ktp_socket;

void init();
void d_init();
void print_error(char *msg);
int k_socket(int domain, int type, int protocol);
int k_bind(int k_sockfd, struct sockaddr_in *src_addr, struct sockaddr_in *dst_addr);
int k_sendto(int k_sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dst_addr, socklen_t addrlen);
int k_recvfrom(int k_sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int k_close(int k_sockfd);

#endif