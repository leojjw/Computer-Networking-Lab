#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/socket.h"
#include "string.h"
#include "arpa/inet.h"
#include "pthread.h"

#define	MAXLINE	 2048           /* Max text line length */
#define MAGIC_NUMBER_LENGTH 6
#define LISTENQ 1024            /* Second argument to listen() */

struct myftp_header {
    char m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                        /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));


/*
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
*/

int open_listenfd(char* port);
void open_connection();
int authentication(char* payload);
void list_files();
void download_files(int filename_length);
void upload_files(int filename_length);
void close_connection();

int connfd;
int authenticate = 0;
//struct myftp_header_auth REPLY;

int main(int argc, char **argv)
{
    int listenfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    
    //struct myftp_header OPEN_CONN_REPLY, AUTH_REPLY;
    //struct myftp_header_auth AUTH_REQUEST;

    
    
    /* Check command-line args */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(1);
    }

    listenfd = open_listenfd(argv[2]);
    //list_files();

    struct myftp_header REPLY;
    char recv_buffer[MAXLINE];
    while (1) {
        clientlen = sizeof(clientaddr);
        //connfd = malloc(sizeof(int));
        connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);
        if (connfd > 0){

            //printf("connected! %d server\n", connfd);
            /*
            while (!read(connfd, &OPEN_CONN_REPLY, sizeof(OPEN_CONN_REPLY)));
            if (!strncmp(OPEN_CONN_REPLY.m_protocol, "\xe3myftp", 6)){
                OPEN_CONN_REPLY.m_type = 0xA2;
                OPEN_CONN_REPLY.m_status = 1;
                //printf("server reveived request header\n");
                write(connfd, &OPEN_CONN_REPLY, sizeof(OPEN_CONN_REPLY));
            }
            */
            open_connection();
            /*
            while (!read(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY)));
            while (!read(connfd, auth, MAXLINE));
            auth[AUTH_REPLY.m_length - 13] = '\0';
            //AUTH_REQUEST.m_payload[AUTH_REPLY.m_length - 13] = '\0';
            if (!strncmp(AUTH_REPLY.m_protocol, "\xe3myftp", 6) && AUTH_REPLY.m_type == 0xA3){
                //memcpy(AUTH_REPLY.m_protocol, "\xe3myftp", 6);
                AUTH_REPLY.m_type = 0xA4;
                AUTH_REPLY.m_length = 12;
                //printf("%ld %s\n", strlen(auth), auth);
                if (!strcmp(auth, "user 123123")){
                    AUTH_REPLY.m_status = 1;
                    write(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY));
                } else {
                    AUTH_REPLY.m_status = 0;
                    write(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY));
                    close(connfd);
                }
                
                //printf("server reveived request header\n");
                //write(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY));
            }
            */
            while (!recv(connfd, recv_buffer, sizeof(recv_buffer), 0));
            memcpy(&REPLY, recv_buffer, sizeof(REPLY));
            //while (!read(connfd, &REPLY, sizeof(REPLY)));
            /*
            printf("yo %s\n", REPLY.m_protocol);
            printf("%d\n", (!strncmp(REPLY.m_protocol, "\xe3myftp", 6)));
            printf("%d\n", (REPLY.m_type == 0xA3));
            printf("%x\n", REPLY.m_type);
            */
            if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xA3){
                //printf("!!!!\n");
                if (!authentication(recv_buffer + sizeof(struct myftp_header))){
                    //printf("???\n");
                    continue;
                }
            } else if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xAB){
                close_connection();
                continue;
            }
            while (1){
                while (!read(connfd, &REPLY, sizeof(REPLY)));
                if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xA5){
                    //printf("go list\n");
                    list_files();
                }
                else if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xA7){
                    download_files(REPLY.m_length - 13);
                }
                else if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xA9){
                    upload_files(REPLY.m_length - 13);
                }
                else if (!strncmp(REPLY.m_protocol, "\xe3myftp", 6) && REPLY.m_type == (char)0xAB){
                    close_connection();
                    break;
                }
            }
            
        //pthread_create(&tid, NULL, thread, connfdp);
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
    struct myftp_header OPEN_CONN_REPLY;

    while (!read(connfd, &OPEN_CONN_REPLY, sizeof(OPEN_CONN_REPLY)));
    if (!strncmp(OPEN_CONN_REPLY.m_protocol, "\xe3myftp", 6)){
        OPEN_CONN_REPLY.m_type = 0xA2;
        OPEN_CONN_REPLY.m_status = 1;
        write(connfd, &OPEN_CONN_REPLY, sizeof(OPEN_CONN_REPLY));
    }

    return;
}

int authentication(char* payload){
    struct myftp_header AUTH_REPLY;
    //char auth[payload_length + 1];

    //while (!read(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY)));
    //while (!read(connfd, auth, payload_length));
    //auth[payload_length] = '\0';
    //auth[payload_length] = '\0';
            //AUTH_REQUEST.m_payload[AUTH_REPLY.m_length - 13] = '\0';
            /*
    if (!strncmp(AUTH_REPLY.m_protocol, "\xe3myftp", 6) && AUTH_REPLY.m_type == 0xA3){
                //memcpy(AUTH_REPLY.m_protocol, "\xe3myftp", 6);
        AUTH_REPLY.m_type = 0xA4;
        AUTH_REPLY.m_length = 12;
                //printf("%ld %s\n", strlen(auth), auth);
        if (!strcmp(auth, "user 123123")){
            AUTH_REPLY.m_status = 1;
            write(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY));
            return 1;
        } else {
            AUTH_REPLY.m_status = 0;
            write(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY));
            close(connfd);
            return 0;
        }
                
                //printf("server reveived request header\n");
                //write(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY));
    }*/
    //bzero(&AUTH_REPLY, sizeof(AUTH_REPLY));
    memcpy(AUTH_REPLY.m_protocol, "\xe3myftp", 6);
    AUTH_REPLY.m_type = 0xA4;
    AUTH_REPLY.m_length = htonl(12);
                //printf("%ld %s\n", strlen(auth), auth);
    if (!strcmp(payload, "user 123123\0")){
        AUTH_REPLY.m_status = 1;
        send(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY), 0);
        return 1;
    } else {
        AUTH_REPLY.m_status = 0;
        send(connfd, &AUTH_REPLY, sizeof(AUTH_REPLY), 0);
        close(connfd);
        return 0;
    }
}

void list_files(){
    struct myftp_header LIST_REPLY;
    FILE *fp;
    char payload[MAXLINE];// = "----- file list start -----\n";
    fp = popen("ls", "r");
    int len = 0;//strlen(payload);
    while(fgets(payload + len, MAXLINE, fp)!= NULL){
        len = strlen(payload);
        payload[len - 1] = '\n';
    }
    fclose(fp);
    payload[strlen(payload)] = '\0';
    //strcat(payload, "----- file list end -----\n\0");

    //bzero(&LIST_REPLY, sizeof(LIST_REPLY));
    memcpy(LIST_REPLY.m_protocol, "\xe3myftp", 6);
    LIST_REPLY.m_type = 0xA6;
    LIST_REPLY.m_length = 12 + strlen(payload) + 1;
    write(connfd, &LIST_REPLY, sizeof(LIST_REPLY));
    write(connfd, payload, strlen(payload));

    return;
    /*
    while (!read(connfd, &LIST_REPLY, sizeof(LIST_REPLY)));
    if (!strncmp(LIST_REPLY.m_protocol, "\xe3myftp", 6) && LIST_REPLY.m_type == 0xA5){
        LIST_REPLY.m_type = 0xA6;
        LIST_REPLY.m_length = 12 + strlen(payload) + 1;
        write(connfd, &LIST_REPLY, sizeof(LIST_REPLY));
        write(connfd, payload, strlen(payload));
    }*/
}

void download_files(int filename_length){
    struct myftp_header GET_REPLY;
    char filename[filename_length + 1];
    while (!read(connfd, filename, filename_length));
    filename[filename_length] = '\0';

    memcpy(GET_REPLY.m_protocol, "\xe3myftp", 6);
    GET_REPLY.m_type = 0xA8;
    GET_REPLY.m_length = 12;
    
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL){
        GET_REPLY.m_status = 0;
        write(connfd, &GET_REPLY, sizeof(GET_REPLY));
        return;
    }
    GET_REPLY.m_status = 1;
    write(connfd, &GET_REPLY, sizeof(GET_REPLY));
    fseek(fp, 0, SEEK_END);
    unsigned long filesize = ftell(fp);
    char *buffer = (char*)malloc(sizeof(char)*filesize);
    rewind(fp);
    // store read data into buffer
    fread(buffer, sizeof(char), filesize, fp);

    GET_REPLY.m_type = 0xFF;
    GET_REPLY.m_length = 12 + (uint32_t)filesize;
    write(connfd, &GET_REPLY, sizeof(GET_REPLY));
    write(connfd, buffer, filesize);
}

void upload_files(int filename_length){

}

void close_connection(){
    close(connfd);
    return;
}