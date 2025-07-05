/*=====================================
Assignment 4 Final Submission
Name: Abhinav Akarsh
Roll number: 22CS30004
initksocket.c
=====================================*/

#include "k_socket.h"

pthread_t R_thread, S_thread, G_thread, C_thread;

int mutex, sem1, sem2, shmid;
struct sembuf pop, vop;
ktp_socket *SM;

// remove semaphores and shared memory and exit
void clean(int status) {
    // Mutex is required here else there may be a segmentation fault
    P(mutex);
    shmdt(SM);
    shmctl(shmid, IPC_RMID, 0);
    semctl(sem1, 0, IPC_RMID, 0);
    semctl(sem2, 0, IPC_RMID, 0);
    semctl(mutex, 0, IPC_RMID, 0);
    exit(status);
}

// function to get semaphores and shared memory
void init() {
    int key = ftok("/", 41);
    if(key == -1) {
        perror("ftok");
        exit(1);
    }
    
    // initialize shared memory
    shmid = shmget(key, MAX_SOCKETS * sizeof(ktp_socket), 0777|IPC_CREAT|IPC_EXCL);
    if(shmid == -1) {
        perror("shmget");
        clean(1);
    }
    SM = (ktp_socket *)shmat(shmid, 0, 0);

    // initialize semaphores
    key = ftok("/", 42);
    mutex = semget(key, 1, 0777|IPC_CREAT|IPC_EXCL);
    if(mutex == -1) {
        perror("semget");
        clean(1);
    }
    semctl(mutex, 0, SETVAL, 1);

    key = ftok("/", 43);
    sem1 = semget(key, 1, 0777|IPC_CREAT|IPC_EXCL);
    if(sem1 == -1) {
        perror("semget1");
        clean(1);
    }
    semctl(sem1, 0, SETVAL, 0);

    key = ftok("/", 44);
    sem2 = semget(key, 1, 0777|IPC_CREAT|IPC_EXCL);
    if(sem2 == -1) {
        perror("semget2");
        clean(1);
    }
    semctl(sem2, 0, SETVAL, 0);

    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;
    return;
}

// signal handler for SIGINT (^C)
void signalHandler(int sig) {
    if(sig == SIGINT) {
        printf("\nExiting...\n");
        clean(0);
    }
    return;
}

// drop message with probability p
int dropMessage(float p) {
    float r = (rand()%100) / 100.0;
    return r < p;
}

// R_thread: receiver thread
void *receiver(void *arg) {
    printf("Receiver thread started...\n");
    fd_set fds;
    struct timeval tv;
    tv.tv_sec = T;
    tv.tv_usec = 0;
    while(1) {
        FD_ZERO(&fds);
        int maxfd = 0;
        P(mutex);
        for(int i=0;i<MAX_SOCKETS;i++) {
            if(SM[i].state==BOUND) {
                FD_SET(SM[i].sockfd, &fds);
                maxfd = (maxfd > SM[i].sockfd) ? maxfd : SM[i].sockfd;
            }
        }
        V(mutex);

        if(select(maxfd+1, &fds, NULL, NULL, &tv) == -1) {
            perror("select");
        }

        P(mutex);
        for(int i=0;i<MAX_SOCKETS;i++) {
            if(SM[i].state==BOUND && FD_ISSET(SM[i].sockfd, &fds)) {
                char packet[BUFLEN+2];
                int len = recvfrom(SM[i].sockfd, packet, BUFLEN+2, 0, NULL, NULL);
                if(len == -1) {
                    perror("recvfrom");
                    continue;
                }
                if(dropMessage(DROP_PROB)) {
                    printf("Socket %d: Dropped packet\n", i);
                    continue;
                }


                // data packet
                if(packet[0] == '0') {
                    
                    int seq = (packet[1] + MAX_SEQ) % MAX_SEQ;
                    int start = SM[i].rwnd.base;
                    int end = (SM[i].rwnd.base + SM[i].rwnd.size) % MAX_SEQ;
                    
                    printf("Socket %d: Received data packet with seq = %d: ", i, seq);
                    // sequence number in window = [start, end)
                    if((start<=end && start<=seq && seq<end) || (start>end && (start<=seq || seq<end))) {
                        int buf_idx = (SM[i].recv_buf.write_end + (seq - start + MAX_SEQ) % MAX_SEQ) % MAX_WND;
                        
                        // duplicate packet
                        if(SM[i].rwnd.marked[seq]) {
                            printf("duplicate\n");
                        }
                        else {  // new packet
                            SM[i].rwnd.marked[seq] = 1;
                            memcpy(SM[i].recv_buf.data[buf_idx], packet+2, len-2);
                            SM[i].recv_buf.len[buf_idx] = len-2;
                            printf("%d bytes\n", len-2);
                        }
                        
                        // slide window
                        // printf("Socket %d: rwnd_base = %d, rwnd_size = %d\n", i, SM[i].rwnd.base, SM[i].rwnd.size);
                        while(SM[i].rwnd.marked[SM[i].rwnd.base] && SM[i].rwnd.base!=end) {
                            SM[i].rwnd.marked[SM[i].rwnd.base] = 0;
                            SM[i].rwnd.base = (SM[i].rwnd.base + 1) % MAX_SEQ;
                            SM[i].recv_buf.write_end = (SM[i].recv_buf.write_end + 1) % MAX_WND;
                            SM[i].recv_buf.count++;
                            SM[i].rwnd.size--;
                        }
                        // printf("Socket %d: rwnd_base = %d, rwnd_size = %d\n", i, SM[i].rwnd.base, SM[i].rwnd.size);
                        
                        // reset flag
                        if(SM[i].flag == NOSPACE) {
                            SM[i].flag = NORMAL;
                            printf("Socket %d: Space available in receive buffer, flag reset to NORMAL\n", i);
                        }
                        // set flag if buffer is full
                        if(SM[i].rwnd.size == 0) {
                            SM[i].flag = NOSPACE;
                            printf("Socket %d: Receive buffer full, flag set to NOSPACE\n", i);
                        }
                    }
                    else {
                        printf("out of window\n");
                    }

                    // send acknowledgement of the last in order received message
                    char ack[3];
                    ack[0] = '1';   // acknowledgement packet
                    ack[1] = (SM[i].rwnd.base + MAX_SEQ - 1) % MAX_SEQ; // sequence number of last inorder received message
                    ack[2] = SM[i].rwnd.size;   // rwnd size
                    sendto(SM[i].sockfd, ack, 3, 0, (struct sockaddr *)&(SM[i].dst_addr), sizeof(struct sockaddr_in));
                    printf("Socket %d: Sent ACK = %d, rwnd = %d\n", i, (ack[1]+MAX_SEQ)%MAX_SEQ, ack[2]);
                }

                // acknowledgement packet
                else {
                    int ack = (packet[1] + MAX_SEQ) % MAX_SEQ;
                    int rwnd = packet[2];
                    int start = SM[i].swnd.base;
                    int end = (SM[i].swnd.base + SM[i].swnd.size) % MAX_SEQ;
                    printf("Socket %d: Received ACK = %d, rwnd = %d\n", i, ack, rwnd);
                    
                    // ack in window
                    if((start<=end && start<=ack && ack<end) || (start>end && (start<=ack || ack<end))) {
                        // slide window
                        // printf("Socket %d: swnd_base = %d, swnd_size = %d, swnd_count = %d\n", i, SM[i].swnd.base, SM[i].swnd.size, SM[i].swnd.count);
                        int x = ((ack - SM[i].swnd.base + MAX_SEQ) % MAX_SEQ) + 1;
                        SM[i].swnd.count -= x;
                        SM[i].swnd.base = (ack + 1) % MAX_SEQ;
                        SM[i].send_buf.read_end = (SM[i].send_buf.read_end + x) % MAX_WND;
                        SM[i].send_buf.count -= x;
                        // printf("Socket %d: swnd_base = %d, swnd_size = %d, swnd_count = %d\n", i, SM[i].swnd.base, rwnd, SM[i].swnd.count);
                    }
                    
                    // update window size
                    SM[i].swnd.size = rwnd;
                    if(SM[i].swnd.size < SM[i].swnd.count) SM[i].swnd.count = SM[i].swnd.size;
                }
            }
        }
        V(mutex);
    }
}

// S_thread: sender thread
void *sender(void *arg) {
    printf("Sender thread started...\n");
    while(1) {
        usleep(T*500000);
        P(mutex);
        for(int i=0;i<MAX_SOCKETS;i++) {
            if(SM[i].state == BOUND) {
                long long int curr_time = time(NULL);

                // Timeout
                if(curr_time - SM[i].last_sent > T) {
                    // send packets from base to nextseqnum
                    for(int j=0;j<SM[i].swnd.count;j++) {
                        int buf_idx = (SM[i].send_buf.read_end+j) % MAX_WND;
                        int wnd_idx = (SM[i].swnd.base+j) % MAX_SEQ;
                        // data packet with sequence number wnd_idx
                        char packet[BUFLEN+2];
                        packet[0] = '0';    // data packet
                        packet[1] = wnd_idx;  // sequence number
                        memcpy(packet+2, SM[i].send_buf.data[buf_idx], SM[i].send_buf.len[buf_idx]);
                        printf("Socket %d: Resending data packet of %d bytes, SEQ = %d\n", i, SM[i].send_buf.len[buf_idx], wnd_idx);
                        sendto(SM[i].sockfd, packet, SM[i].send_buf.len[buf_idx]+2, 0, (struct sockaddr *)&(SM[i].dst_addr), sizeof(struct sockaddr_in));
                        SM[i].trans_cnt++;
                        SM[i].last_sent = curr_time;
                    }
                }

                // Pending messages
                if(SM[i].send_buf.count > SM[i].swnd.count) {
                    for(int j=SM[i].swnd.count;j<SM[i].send_buf.count;j++) {
                        int buf_idx = (SM[i].send_buf.read_end+j) % MAX_WND;
                        int wnd_idx = (SM[i].swnd.base+j) % MAX_SEQ;
                        if(wnd_idx == (SM[i].swnd.base+SM[i].swnd.size) % MAX_SEQ) break;
                        // data packet with sequence number wnd_idx
                        char packet[BUFLEN+2];
                        packet[0] = '0';    // data packet
                        packet[1] = wnd_idx;  // sequence number
                        memcpy(packet+2, SM[i].send_buf.data[buf_idx], SM[i].send_buf.len[buf_idx]);
                        printf("Socket %d: Sending data packet of %d bytes, SEQ = %d\n", i, SM[i].send_buf.len[buf_idx], wnd_idx);
                        sendto(SM[i].sockfd, packet, SM[i].send_buf.len[buf_idx]+2, 0, (struct sockaddr *)&(SM[i].dst_addr), sizeof(struct sockaddr_in));
                        SM[i].swnd.count++;
                        SM[i].trans_cnt++;
                        SM[i].last_sent = curr_time;
                    }
                }

                // send DUPACK with updated rwnd size
                if(SM[i].flag==NOSPACE && SM[i].rwnd.size>0) {
                    char ack[3];
                    ack[0] = '1';   // acknowledgement packet
                    ack[1] = (SM[i].rwnd.base + MAX_SEQ - 1) % MAX_SEQ; // sequence number of last inorder received message
                    ack[2] = SM[i].rwnd.size;   // rwnd size
                    sendto(SM[i].sockfd, ack, 3, 0, (struct sockaddr *)&(SM[i].dst_addr), sizeof(struct sockaddr_in));
                    printf("Socket %d: Sent DUPACK = %d, rwnd = %d\n", i, (ack[1]+MAX_SEQ)%MAX_SEQ, ack[2]);
                    // flag is not reset here, it will be reset when atleast one message is received
                }
            }
        }
        V(mutex);
    }
    return NULL;
}

// C_thread: create, bind and close sockets
void *createBindClose(void *arg) {
    printf("Create-Bind-Close thread started...\n");
    while(1) {
        P(sem1);
        for(int i=0;i<MAX_SOCKETS;i++) {
            if(SM[i].state == TO_CREATE_SOCKET) {
                SM[i].sockfd = socket(AF_INET, SOCK_DGRAM, 0);
                if(SM[i].sockfd == -1) {
                    perror("socket");
                    SM[i].error = errno;
                    break;
                }
                SM[i].state = SOCKET_CREATED;
                SM[i].swnd.base = 0;
                SM[i].swnd.size = MAX_WND;
                SM[i].swnd.count = 0;
                SM[i].rwnd.base = 0;
                SM[i].rwnd.size = MAX_WND;
                SM[i].rwnd.count = 0;
                SM[i].flag = NORMAL;
                SM[i].send_buf.read_end = 0;
                SM[i].send_buf.write_end = 0;
                SM[i].send_buf.count = 0;
                SM[i].recv_buf.read_end = 0;
                SM[i].recv_buf.write_end = 0;
                SM[i].recv_buf.count = 0;
                SM[i].last_sent = 0;
                SM[i].trans_cnt = 0;
                SM[i].msg_count = 0;
                SM[i].error = SUCCESS;
                for(int j=0;j<MAX_SEQ;j++) SM[i].rwnd.marked[j] = 0;
                printf("Socket %d: Socket created: %d\n", i, SM[i].sockfd);
                break;
            }
            else if(SM[i].state == TO_BIND) {
                struct sockaddr_in src_addr = SM[i].src_addr;
                struct sockaddr_in dst_addr = SM[i].dst_addr;
                if(bind(SM[i].sockfd, (struct sockaddr *)&src_addr, sizeof(struct sockaddr_in)) == -1) {
                    perror("bind");
                    SM[i].error = errno;
                    break;
                }
                SM[i].state = BOUND;
                SM[i].error = SUCCESS;
                printf("Socket %d: Bound to source %s:%d, destination %s:%d\n", i, inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), inet_ntoa(dst_addr.sin_addr), ntohs(dst_addr.sin_port));
                break;
            }
            else if(SM[i].state == TO_CLOSE) {
                if(close(SM[i].sockfd) == -1) {
                    perror("close");
                    SM[i].error = errno;
                    break;
                }
                SM[i].state = FREE;
                SM[i].error = SUCCESS;
                printf("\n=================STATS FOR SOCKET: %d================\n", i);
                printf("\tProbability of dropping a packet: %.0f%%\n", 100*DROP_PROB);
                printf("\tNumber of messages sent: %d\n", SM[i].msg_count);
                printf("\tNumber of transmissions: %d\n", SM[i].trans_cnt);
                printf("\tTransmissions per message: ");
                if(SM[i].msg_count) printf("%.2f\n", (float)(SM[i].trans_cnt)/SM[i].msg_count);
                else printf("na\n");
                printf("=====================================================\n\n");
                printf("Socket %d: Socket closed\n", i);
                break;
            }
        }
        V(sem2);
    }
}

// G_thread: garbage collector thread
void *garbageCollector(void *arg) {
    printf("Garbage collector thread started...\n");
    while(1) {
        sleep(2*T);
        P(mutex);
        for(int i=0;i<MAX_SOCKETS;i++) {
            if(SM[i].state != FREE) {
                // Check if process exists
                if(kill(SM[i].pid, 0) == -1) {
                    printf("Process %d exited: Closing socket %d\n", SM[i].pid, i);
                    close(SM[i].sockfd);
                    SM[i].state = FREE;
                }
            }
        }
        V(mutex);
    }
    shmdt(SM);
    exit(0);
}

int main() {
    srand(time(NULL));
    signal(SIGINT, signalHandler);  // handle ^C
    init(); // make semaphores and shared memory

    // mark all sockets as free
    for(int i=0;i<MAX_SOCKETS;i++) {
        P(mutex);
        SM[i].state = FREE;
        V(mutex);
    }

    printf("KTP initialized...\n");

    // create threads
    pthread_create(&R_thread, NULL, receiver, NULL);
    pthread_create(&S_thread, NULL, sender, NULL);
    pthread_create(&G_thread, NULL, garbageCollector, NULL);
    pthread_create(&C_thread, NULL, createBindClose, NULL);

    // wait for all threads to finish
    pthread_join(R_thread, NULL);
    pthread_join(S_thread, NULL);
    pthread_join(G_thread, NULL);
    pthread_join(C_thread, NULL);

    // clean up
    clean(0);

    return 0;
}