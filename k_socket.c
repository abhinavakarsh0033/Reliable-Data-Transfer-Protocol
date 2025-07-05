/*=====================================
Assignment 4 Final Submission
Name: Abhinav Akarsh
Roll number: 22CS30004
k_socket.c
=====================================*/

#include "k_socket.h"

int error = SUCCESS;
struct sembuf pop, vop;
int mutex, sem1, sem2, shmid;
ktp_socket *SM;

void init() {
    int key = ftok("/", 41);
    if(key == -1) {
        perror("ftok");
        exit(1);
    }

    // initialize shared memory
    shmid = shmget(key, MAX_SOCKETS * sizeof(ktp_socket), 0777);
    if(shmid == -1) {
        perror("shmget");
        exit(1);
    }
    SM = (ktp_socket *)shmat(shmid, 0, 0);

    // initialize semaphores
    key = ftok("/", 42);
    mutex = semget(key, 1, 0777);
    if(mutex == -1) {
        perror("semget");
        exit(1);
    }

    key = ftok("/", 43);
    sem1 = semget(key, 1, 0777);
    if(sem1 == -1) {
        perror("semget1");
        exit(1);
    }

    key = ftok("/", 44);
    sem2 = semget(key, 1, 0777);
    if(sem2 == -1) {
        perror("semget2");
        exit(1);
    }

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    return;
}

void d_init() {
    shmdt(SM);
    return;
}

void print_error(char *msg) {
    switch(error) {
        case SUCCESS:
            fprintf(stderr, "%s: Success\n", msg);
            break;
        case ENOSPACE:
            fprintf(stderr, "%s: No space available\n", msg);
            break;
        case ENOTBOUND:
            fprintf(stderr, "%s: Destination address not bound\n", msg);
            break;
        case ENOMESSAGE:
            fprintf(stderr, "%s: No message available\n", msg);
            break;
        case EINVALIDTYPE:
            fprintf(stderr, "%s: Invalid socket type\n", msg);
            break;
        case EINVALIDSOCKET:
            fprintf(stderr, "%s: Invalid socket fd\n", msg);
            break;
        default: {
            if(!errno) errno = error;
            perror(msg);
        }
    }
    return;
}

int k_socket(int domain, int type, int protocol) {
    init();
    if(type != SOCK_KTP) {
        error = EINVALIDTYPE;
        d_init();
        return -1;
    }

    P(mutex);

    for(int i=0; i<MAX_SOCKETS; i++) {
        if(SM[i].state == FREE) {
            SM[i].state = TO_CREATE_SOCKET;
            SM[i].pid = getpid();
            V(sem1);    // signal init to create socket
            P(sem2);    // wait for init to create socket

            if(SM[i].state != SOCKET_CREATED) {
                V(mutex);
                d_init();
                return -1;
            }

            V(mutex);
            d_init();

            printf("Socket created: %d\n", i);

            error = SUCCESS;
            return i;
        }
    }
    V(mutex);

    error = ENOSPACE;
    d_init();
    return -1;
}

int k_bind(int k_sockfd, struct sockaddr_in *src_addr, struct sockaddr_in *dst_addr) {
    init();
    P(mutex);
    if(k_sockfd<0 || k_sockfd>=MAX_SOCKETS || SM[k_sockfd].state!=SOCKET_CREATED) {
        error = EINVALIDSOCKET;
        V(mutex);
        d_init();
        return -1;
    }
    
    SM[k_sockfd].src_addr = *src_addr;
    SM[k_sockfd].dst_addr = *dst_addr;

    SM[k_sockfd].state = TO_BIND;
    V(sem1);    // signal init to bind socket
    P(sem2);    // wait for init to bind socket

    if(SM[k_sockfd].state != BOUND) {
        error = SM[k_sockfd].error;
        V(mutex);
        d_init();
        return -1;
    }

    printf("Socket %d bound to source %s:%d, destination %s:%d\n", k_sockfd, inet_ntoa(src_addr->sin_addr), ntohs(src_addr->sin_port), inet_ntoa(dst_addr->sin_addr), ntohs(dst_addr->sin_port));

    V(mutex);
    d_init();
    error = SUCCESS;
    return 0;
}

int k_sendto(int k_sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dst_addr, socklen_t addrlen) {
    init();
    P(mutex);
    if(k_sockfd<0 || k_sockfd>=MAX_SOCKETS || SM[k_sockfd].state!=BOUND) {
        error = EINVALIDSOCKET;
        V(mutex);
        d_init();
        return -1;
    }
    int sockfd = SM[k_sockfd].sockfd;

    // Check if destination ip and port are same as bound ip and port
    if(SM[k_sockfd].dst_addr.sin_addr.s_addr != ((struct sockaddr_in *)dst_addr)->sin_addr.s_addr || SM[k_sockfd].dst_addr.sin_port != ((struct sockaddr_in *)dst_addr)->sin_port) {
        error = ENOTBOUND;
        V(mutex);
        d_init();
        return -1;
    }

    // Check if send buffer is free
    if(SM[k_sockfd].send_buf.count == MAX_WND) {
        error = ENOSPACE;
        V(mutex);
        d_init();
        return -1;
    }

    // Insert the message in send buffer
    len = len > BUFLEN ? BUFLEN : len;
    int write_end = SM[k_sockfd].send_buf.write_end;
    SM[k_sockfd].send_buf.len[write_end] = len;
    memcpy(SM[k_sockfd].send_buf.data[write_end], buf, len);
    SM[k_sockfd].send_buf.write_end = (write_end + 1) % MAX_WND;
    SM[k_sockfd].send_buf.count++;
    SM[k_sockfd].msg_count++;

    printf("Sent message: %ld Bytes\n", len);

    V(mutex);
    d_init();
    error = SUCCESS;
    return len;
}

int k_recvfrom(int k_sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    init();
    P(mutex);
    if(k_sockfd<0 || k_sockfd>=MAX_SOCKETS || SM[k_sockfd].state!=BOUND) {
        error = EINVALIDSOCKET;
        V(mutex);
        d_init();
        return -1;
    }

    // Check if there is any message in receive buffer
    if(SM[k_sockfd].recv_buf.count == 0) {
        error = ENOMESSAGE;
        V(mutex);
        d_init();
        return -1;
    }

    // Copy the message
    int read_end = SM[k_sockfd].recv_buf.read_end;
    len = len > SM[k_sockfd].recv_buf.len[read_end] ? SM[k_sockfd].recv_buf.len[read_end] : len;
    memcpy(buf, SM[k_sockfd].recv_buf.data[read_end], len);

    // Remove the message from receive buffer
    SM[k_sockfd].recv_buf.len[read_end] = 0;
    SM[k_sockfd].recv_buf.read_end = (read_end + 1) % MAX_WND;
    SM[k_sockfd].recv_buf.count--;
    SM[k_sockfd].rwnd.size++;

    if(src_addr != NULL) *src_addr = *((struct sockaddr *)&(SM[k_sockfd].dst_addr));
    if(addrlen != NULL) *addrlen = sizeof(struct sockaddr_in);

    printf("Received message: %ld Bytes\n", len);

    V(mutex);
    d_init();
    error = SUCCESS;
    return len;
}

int k_close(int k_sockfd) {
    init();
    P(mutex);
    if(k_sockfd<0 || k_sockfd>=MAX_SOCKETS || SM[k_sockfd].state==FREE) {
        error = EINVALIDSOCKET;
        V(mutex);
        d_init();
        return -1;
    }
    V(mutex);
    
    // wait for sometime for send buffer to be empty
    if(SM[k_sockfd].send_buf.count > 0) {
        long long int timeout = time(NULL) + 20*T;
        printf("Waiting for send buffer to be cleared, Remaining messages: %d\n", SM[k_sockfd].send_buf.count);
        printf("The socket will be closed within %d seconds\n", 20*T);
        while(timeout > time(NULL)) {
            P(mutex);
            if(SM[k_sockfd].send_buf.count == 0) {
                printf("Send buffer empty\n");
                V(mutex);
                break;
            }
            V(mutex);
            usleep(100);
        }
    }

    // Close the socket file descriptor
    P(mutex);
    SM[k_sockfd].state = TO_CLOSE;
    V(sem1);    // signal init to close socket
    P(sem2);    // wait for init to close socket

    if(SM[k_sockfd].state != FREE) {
        error = SM[k_sockfd].error;
        V(mutex);
        d_init();
        return -1;
    }

    printf("Socket %d closed\n", k_sockfd);

    V(mutex);
    d_init();
    error = SUCCESS;
    return 0;
}