#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/socket.h"
#include "netdb.h"
#include "arpa/inet.h"
#include "string.h"
#include "pthread.h"

#define	MAXLINE	 2048                     /* Max text line length */
#define MAGIC_NUMBER_LENGTH 6

int clientfd;
int client_status = 0;                    /* 0,1,2 represent "IDLE", "Authentication", "MAIN" respectively */

struct myFTP_Header {
    char m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                        /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));


void open_clientfd(char* host, char* port);
void authentication(char* payload);
void list_files();
void download_files(char* filename);
void upload_files(char* filename);
void close_connection();

int main(int argc, char **argv)
{
    char *host, *port, *payload, buf[MAXLINE];

    char delim[] = " ";
    
    while (fgets(buf, MAXLINE, stdin) != NULL) {

        buf[strlen(buf) - 1] = ' ';
        char* cmd = strtok(buf, delim);
        
        if (!strcmp(cmd, "open")){
            if (!(client_status == 0)){
                printf("Connection already exists.\n");
                continue;
            }
            host = strtok(NULL, delim);
            port = strtok(NULL, delim);
            open_clientfd(host, port);
            continue;
        }
        
        else if (!strcmp(cmd, "auth")){
            if (client_status == 0){
                printf("No connection existing.\n");
                continue;
            }
            if (client_status == 2){
                printf("Authentication already granted.\n");
                continue;
            }
            payload = strtok(NULL, delim);
            strtok(NULL, delim);
            authentication(payload);
            continue;
        }

        else if (!strcmp(cmd, "ls")){
            if (client_status == 0){
                printf("No connection existing.\n");
                continue;
            }
            if (client_status == 1){
                printf("ERROR: authentication not started. Please issue an authentication command.\n");
                continue;
            }
            list_files();
            continue;
        }

        else if (!strcmp(cmd, "get")){
            if (client_status == 0){
                printf("No connection existing.\n");
                continue;
            }
            if (client_status == 1){
                printf("ERROR: authentication not started. Please issue an authentication command.\n");
                continue;
            }
            payload = strtok(NULL, delim);
            download_files(payload);
            continue;
        }

        else if (!strcmp(cmd, "put")){
            if (client_status == 0){
                printf("No connection existing.\n");
                continue;
            }
            if (client_status == 1){
                printf("ERROR: authentication not started. Please issue an authentication command.\n");
                continue;
            }
            payload = strtok(NULL, delim);
            upload_files(payload);
            continue;
        }
        
        else if (!strcmp(cmd, "quit")){
            if (client_status == 0){
                exit(0);
            }
            close_connection();
            continue;
        }
    }
    exit(0);
}

 void open_clientfd(char *host, char *port) {
    struct sockaddr_in servaddr;
    struct myFTP_Header OPEN_CONN_REQUEST;

    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, host, &servaddr.sin_addr);

    if (!connect(clientfd, (struct sockaddr *) &servaddr, sizeof(servaddr))){
        memcpy(OPEN_CONN_REQUEST.m_protocol, "\xe3myftp", 6);
        OPEN_CONN_REQUEST.m_type = 0xA1;
        OPEN_CONN_REQUEST.m_length = htonl(12);
        write(clientfd, &OPEN_CONN_REQUEST, sizeof(OPEN_CONN_REQUEST));

        while (!read(clientfd, &OPEN_CONN_REQUEST, sizeof(OPEN_CONN_REQUEST)));
        
        if (OPEN_CONN_REQUEST.m_status == 1){
            printf("Server connection accepted.\n");
            client_status = 1;
            return;
        }
    }
    printf("connection error\n");
    return;
 }

 void authentication(char* payload){
    struct myFTP_Header AUTH_REQUEST;
    char send_buffer[MAXLINE];

    payload[strlen(payload)] = ' ';

    int len = 12 + strlen(payload) + 1;
    memcpy(AUTH_REQUEST.m_protocol, "\xe3myftp", 6);
    AUTH_REQUEST.m_type = 0xA3;
    AUTH_REQUEST.m_length = htonl(len);

    memcpy(send_buffer, &AUTH_REQUEST, sizeof(AUTH_REQUEST));
    strcpy(send_buffer + sizeof(AUTH_REQUEST), payload);
    write(clientfd, send_buffer, len);
    
    while((!read(clientfd, &AUTH_REQUEST, sizeof(AUTH_REQUEST))));

    if (!strncmp(AUTH_REQUEST.m_protocol, "\xe3myftp", 6) && (AUTH_REQUEST.m_type == (char)0xA4)){
        if (AUTH_REQUEST.m_status == 1){
            printf("Authentication granted.\n");
            client_status = 2;
            return;
        } else {
            printf("ERROR: Authentication rejected. Connection closed.\n");
            client_status = 0;
            close(clientfd);
            return;
        }
    }
 }

 void list_files(){
    struct myFTP_Header LIST_REQUEST;
    char list[MAXLINE];

    memcpy(LIST_REQUEST.m_protocol, "\xe3myftp", 6);
    LIST_REQUEST.m_type = 0xA5;
    LIST_REQUEST.m_length = htonl(12);
    write(clientfd, &LIST_REQUEST, sizeof(LIST_REQUEST));

    while (!read(clientfd, &LIST_REQUEST, sizeof(LIST_REQUEST)));

    size_t ret = 0, len = ntohl(LIST_REQUEST.m_length) - 12;
    while (ret < len) {
        size_t b = read(clientfd, list + ret, len - ret);
        if (b == 0) {printf("Socket closed.\n"); break;}
        if (b < 0) {printf("Error\n"); break;}
        ret += b;
    }
    printf("----- file list start -----\n%s----- file list end -----\n", list);

    return;
 }

void download_files(char* payload){
    struct myFTP_Header GET_REQUEST;
    char* buffer = (char*)malloc(MAXLINE);

    int len = 12 + strlen(payload) + 1;
    memcpy(GET_REQUEST.m_protocol, "\xe3myftp", 6);
    GET_REQUEST.m_type = 0xA7;
    GET_REQUEST.m_length = htonl(len);

    memcpy(buffer, &GET_REQUEST, sizeof(GET_REQUEST));
    strcpy(buffer + sizeof(GET_REQUEST), payload);
    write(clientfd, buffer, len);
    free(buffer);

    while (!read(clientfd, &GET_REQUEST, sizeof(GET_REQUEST)));

    if (GET_REQUEST.m_status == 1){

        while (!read(clientfd, &GET_REQUEST, sizeof(GET_REQUEST)));

        size_t ret = 0, file_size = ntohl(GET_REQUEST.m_length) - 12;
        buffer = (char*)malloc(sizeof(char)*file_size);

        while (ret < file_size) {
            size_t b = read(clientfd, buffer + ret, file_size - ret);
            if (b == 0) {printf("Socket closed.\n"); break;}
            if (b < 0) {printf("Error\n"); break;}
            ret += b;
        }

        FILE *fp;
        fp = fopen(payload, "wb");
        fwrite(buffer, 1, file_size, fp);
        fclose(fp);
        free(buffer);
        printf("File downloaded.\n");
    } else {
        printf("The file does not exist.\n");
    }

    return;
}

void upload_files(char* payload){
    FILE *fp = fopen(payload, "rb");
    if (fp == NULL){
        printf("The file does not exist.\n");
        return;
    }

    struct myFTP_Header PUT_REQUEST;
    char* buffer = (char*)malloc(MAXLINE);

    int len = 12 + strlen(payload) + 1;
    memcpy(PUT_REQUEST.m_protocol, "\xe3myftp", 6);
    PUT_REQUEST.m_type = 0xA9;
    PUT_REQUEST.m_length = htonl(len);

    memcpy(buffer, &PUT_REQUEST, sizeof(PUT_REQUEST));
    strcpy(buffer + sizeof(PUT_REQUEST), payload);
    write(clientfd, buffer, len);
    free(buffer);

    while (!read(clientfd, &PUT_REQUEST, sizeof(PUT_REQUEST)));

    fseek(fp, 0, SEEK_END);
    unsigned long file_size = ftell(fp);
    size_t ret = 0, length = 12 + file_size;
    buffer = (char*)malloc(sizeof(char)*length);
    rewind(fp);
    fread(buffer + 12, sizeof(char), file_size, fp);
    fclose(fp);

    PUT_REQUEST.m_type = 0xFF;
    PUT_REQUEST.m_length = htonl((uint32_t)length);
    memcpy(buffer, &PUT_REQUEST, sizeof(PUT_REQUEST));
    //write(clientfd, buffer, sizeof(PUT_REQUEST) + file_size);
    while (ret < length) {
        size_t b = write(clientfd, buffer + ret, length - ret);
        if (b == 0) {printf("Socket closed.\n"); break;}
        if (b < 0) {printf("Error\n"); break;}
        ret += b;
    }
    printf("File uploaded.\n");
    free(buffer);

    return;
}

void close_connection(){
    struct myFTP_Header QUIT_REQUEST;

    memcpy(QUIT_REQUEST.m_protocol, "\xe3myftp", 6);
    QUIT_REQUEST.m_type = 0xAB;
    QUIT_REQUEST.m_length = htonl(12);
    write(clientfd, &QUIT_REQUEST, sizeof(QUIT_REQUEST));

    while(!read(clientfd, &QUIT_REQUEST, sizeof(QUIT_REQUEST)));

    close(clientfd);
    client_status = 0;
    printf("Thank you.\n");

    return;
}
