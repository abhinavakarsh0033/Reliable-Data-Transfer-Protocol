/*=====================================
Assignment 4 Final Submission
Name: Abhinav Akarsh
Roll number: 22CS30004
user2.c
=====================================*/

#include "k_socket.h"

int main(int argc, char **argv) {
    int sport, dport;
    if(argc < 3) {
        printf("Run with source port and destination port as arguments\n");
        exit(1);
    }
    sport = atoi(argv[1]);
    dport = atoi(argv[2]);

    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if(sockfd == -1) {
        print_error("k_socket");
        exit(1);
    }

    struct sockaddr_in src_addr, dst_addr;
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(sport);
    src_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(dport);
    dst_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(k_bind(sockfd, &src_addr, &dst_addr) == -1) {
        print_error("k_bind");
        exit(1);
    }
    printf("Receiving file...\n");

    char filename[20];
    sprintf(filename, "recv_%d.txt", sport);
    char buf[BUFLEN+1];
    FILE *fp = fopen(filename, "w");
    if(fp == NULL) {
        perror("fopen");
        exit(1);
    }
    while(1) {
        int len;
        while((len = k_recvfrom(sockfd, buf, BUFLEN, 0, NULL, NULL)) == -1);
        if(len == -1) {
            print_error("k_recvfrom");
            exit(1);
        }
        buf[len] = '\0';
        if(!strcmp(buf, "FINISH")) break;
        fwrite(buf, 1, len, fp);
    }
    fclose(fp);
    printf("File received\n");
    
    if(k_close(sockfd) == -1) {
        print_error("k_close");
        exit(1);
    }

    return 0;
}