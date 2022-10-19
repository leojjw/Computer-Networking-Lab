#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/socket.h"
#include "string.h"
#include "arpa/inet.h"
#include "pthread.h"

#define	MAXLINE	 2048                     /* Max text line length */
#define MAGIC_NUMBER_LENGTH 6
#define LISTENQ 1024                      /* Second argument to listen() */

int connfd;

struct myFTP_Header {
    char m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                        /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));

int open_listenfd(char* port);
void open_connection();
int authentication(char* payload);
void list_files();
void download_files(char* filename);
void upload_files(char* filename);
void close_connection();

int main(int argc, char **argv)
{
    /* Check command-line args */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(1);
    }

    int listenfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;

    listenfd = open_listenfd(argv[2]);

    struct myFTP_Header REPLY;
    uint32_t message_length;
    char recv_buffer[MAXLINE];

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);
        if (connfd > 0){

            open_connection();
            
            while (1){
                while (!read(connfd, &REPLY, sizeof(REPLY)));

                size_t size = 12;
                message_length = ntohl(REPLY.m_length);
                while (size < message_length) {
                    size_t b = read(connfd, recv_buffer + size - 12, MAXLINE);
                    if (b == 0) {printf("Socket closed.\n"); break;}
                    if (b < 0) {printf("Error\n"); break;}
                    size += b;
                }

                if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xA3){
                    if (!authentication(recv_buffer)) break;
                }
                else if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xA5){
                    list_files();
                }
                else if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xA7){
                    download_files(recv_buffer);
                }
                else if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xA9){
                    upload_files(recv_buffer);
                }
                else if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xAB){
                    close_connection();
                    break;
                }
            }
        }
    }

    exit(0);
}

int open_listenfd(char* port){
    int listenfd;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(port));
    bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(listenfd, LISTENQ);
    
    return listenfd;
}

void open_connection(){
    struct myFTP_Header OPEN_CONN_REPLY;

    while (!read(connfd, &OPEN_CONN_REPLY, sizeof(OPEN_CONN_REPLY)));

    if (!strncmp(OPEN_CONN_REPLY.m_protocol, "\xe3myftp", 6)){
        OPEN_CONN_REPLY.m_type = 0xA2;
        OPEN_CONN_REPLY.m_status = 1;
        write(connfd, &OPEN_CONN_REPLY, sizeof(OPEN_CONN_REPLY));
    }

    return;
}

int authentication(char* payload){
    struct myFTP_Header AUTH_REPLY;

    memcpy(AUTH_REPLY.m_protocol, "\xe3myftp", 6);
    AUTH_REPLY.m_type = 0xA4;
    AUTH_REPLY.m_length = htonl(12);
    
    if (!strcmp(payload, "user 123123\0")){
        AUTH_REPLY.m_status = 1;
        write(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY));
        return 1;
    } else {
        AUTH_REPLY.m_status = 0;
        write(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY));
        close(connfd);
        return 0;
    }
}

void list_files(){
    struct myFTP_Header LIST_REPLY;
    char send_buffer[MAXLINE];
    FILE *fp;
    
    fp = popen("ls", "r");
    int len = 0;
    char* payload = send_buffer + 12;

    while(fgets(payload + len, MAXLINE, fp)!= NULL){
        len = strlen(payload);
        payload[len - 1] = '\n';
    }
    fclose(fp);
    payload[strlen(payload)] = '\0';
    len = 12 + strlen(payload) + 1;

    memcpy(LIST_REPLY.m_protocol, "\xe3myftp", 6);
    LIST_REPLY.m_type = 0xA6;
    LIST_REPLY.m_length = htonl(len);
    memcpy(send_buffer, &LIST_REPLY, sizeof(LIST_REPLY));
    write(connfd, send_buffer, len);

    return;
}

void download_files(char* filename){
    struct myFTP_Header GET_REPLY;

    memcpy(GET_REPLY.m_protocol, "\xe3myftp", 6);
    GET_REPLY.m_type = 0xA8;
    GET_REPLY.m_length = htonl(12);
    
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL){
        GET_REPLY.m_status = 0;
        write(connfd, &GET_REPLY, sizeof(GET_REPLY));
        return;
    }

    GET_REPLY.m_status = 1;
    write(connfd, &GET_REPLY, sizeof(GET_REPLY));
    fseek(fp, 0, SEEK_END);
    unsigned long file_size = ftell(fp);
    char *buffer = (char*)malloc(sizeof(char)*(file_size + 12));
    rewind(fp);
    fread(buffer + 12, sizeof(char), file_size, fp);

    GET_REPLY.m_type = 0xFF;
    GET_REPLY.m_length = htonl(12 + (uint32_t)file_size);
    memcpy(buffer, &GET_REPLY, sizeof(GET_REPLY));
    write(connfd, buffer, sizeof(GET_REPLY) + file_size);
    free(buffer);

    return;
}

void upload_files(char* payload){
    struct myFTP_Header PUT_REPLY;

    memcpy(PUT_REPLY.m_protocol, "\xe3myftp", 6);
    PUT_REPLY.m_type = 0xAA;
    PUT_REPLY.m_length = htonl(12);
    write(connfd, &PUT_REPLY, sizeof(PUT_REPLY));
    
    while (!read(connfd, &PUT_REPLY, sizeof(PUT_REPLY)));

    size_t file_size = ntohl(PUT_REPLY.m_length) - 12, size = 0;
    char* buffer = (char*)malloc(sizeof(char)*file_size);

    while (size < file_size) {
        size_t b = read(connfd, buffer + size, file_size - size);
        if (b == 0) {printf("Socket closed.\n"); break;}
        if (b < 0) {printf("Error\n"); break;}
        size += b;
    }

    FILE *fp;
    fp = fopen(payload, "wb");
    fwrite(buffer, 1, file_size, fp);
    fclose(fp);
    free(buffer);

    return;
}

void close_connection(){
    struct myFTP_Header QUIT_REPLY;

    memcpy(QUIT_REPLY.m_protocol, "\xe3myftp", 6);
    QUIT_REPLY.m_type = 0xAC;
    QUIT_REPLY.m_length = htonl(12);
    write(connfd, &QUIT_REPLY, sizeof(QUIT_REPLY));
    close(connfd);
    
    return;
}