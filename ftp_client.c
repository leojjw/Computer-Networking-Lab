#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/socket.h"
//#include "sys/types.h"
#include "netdb.h"
#include "arpa/inet.h"
#include "string.h"
#include "pthread.h"

#define	MAXLINE	 2048  /* Max text line length */
#define MAGIC_NUMBER_LENGTH 6

int clientfd;
int client_status = 0;                 /* 0,1,2 represent "IDLE", "Authentication", "MAIN" respectively */

struct myftp_header {
    char m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                        /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed));


struct myftp_header_auth {
    char m_protocol[MAGIC_NUMBER_LENGTH]; 
    char m_type;                        
    char m_status;                      
    uint32_t m_length;    
} __attribute__ ((packed));


void open_clientfd(char* host, char* port);
void authentication(char* payload);
void list_files();
void download_files(char* filename);
void upload_files(char* filename);
void close_connection();

void check_connection();

int main(int argc, char **argv)
{
    /*
    char buff[MAXLINE];
    while (fgets(buff, MAXLINE, stdin) != NULL) {
        printf("%s\n", buff);
    }
    char ex[20] = "au\nau\n0123";
    ex[6] = '\0';
    printf("%ld %s\n", strlen(ex), ex);
    exit(0);
    */

    //printf("%ld\n", sizeof(struct myftp_header_auth));
    char *host, *port, *payload, *p2, buf[MAXLINE];

    char delim[] = " ";

    //int i = 0;
    
    while (fgets(buf, MAXLINE, stdin) != NULL) {
        //printf("%ld", strlen(buf));
        /*
        i = 0;
        while(buf[i] != '\n')
            i++;
        buf[i] = '\0';
        */
        buf[strlen(buf) - 1] = ' ';
        char* cmd = strtok(buf, delim);
        //printf("%s\n", cmd);
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
            //password = strtok(NULL, delim);
            //printf("%ld %ld\n", strlen(username), strlen(password));
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
    //Close(clientfd);
    exit(0);
}

 void open_clientfd(char *host, char *port) {
    struct sockaddr_in servaddr;
    struct myftp_header OPEN_CONN_REQUEST;

    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, host, &servaddr.sin_addr);
    if (!connect(clientfd, (struct sockaddr *) &servaddr, sizeof(servaddr))){
        //bzero(&OPEN_CONN_REQEST, sizeof(OPEN_CONN_REQEST));
        memcpy(OPEN_CONN_REQUEST.m_protocol, "\xe3myftp", 6);
        OPEN_CONN_REQUEST.m_type = 0xA1;
        OPEN_CONN_REQUEST.m_length = htonl(12);
        write(clientfd, &OPEN_CONN_REQUEST, sizeof(OPEN_CONN_REQUEST));
        while (!read(clientfd, &OPEN_CONN_REQUEST, sizeof(OPEN_CONN_REQUEST)));
        //printf("yo %s\n", OPEN_CONN_REQEST.m_protocol);
        client_status= 1;
        if (OPEN_CONN_REQUEST.m_status == 1){
            
            printf("Server connection accepted.\n");
            return;
        }
    }
    printf("connection error\n");
    return;
 }

 void authentication(char* payload){
    struct myFTP_message{
        struct myftp_header AUTH_REQUEST;
        char payload[50];
    } AUTH_MESSAGE;
    struct myftp_header AUTH_REPLY;
    char send_buffer[MAXLINE];

    payload[strlen(payload)] = ' ';
    /*
    int i = 0, j = 0;
    while (payload[i] > 32 && payload[i] < 127){
        AUTH_MESSAGE.payload[i] = payload[i];
        i++;
    }
    AUTH_MESSAGE.payload[i] = ' ';
    i++;
    while (password[j] > 32 && password[j] < 127){
        AUTH_MESSAGE.payload[i] = password[j];
        i++; j++;
    }
    AUTH_MESSAGE.payload[i] = '\0';
    */

    strcpy(AUTH_MESSAGE.payload, payload);
    int len = 12 + strlen(AUTH_MESSAGE.payload) + 1;

    memcpy(AUTH_MESSAGE.AUTH_REQUEST.m_protocol, "\xe3myftp", 6);
    AUTH_MESSAGE.AUTH_REQUEST.m_type = 0xA3;
    AUTH_MESSAGE.AUTH_REQUEST.m_length = htonl(len);
    
    memcpy(send_buffer, &AUTH_MESSAGE, len);
    //send_buffer[len - 1] = '\0';

    size_t size = 0;
    while (size < len) {
        size_t b = send(clientfd, send_buffer + size, len - size, 0);
        if (b == 0) {printf("socket Closed"); break;} // 当连接断开
        if (b < 0) {printf("Error ?"); break;} // 这里可能发生了一些意料之外的情况
        size += b; // 成功将b个byte塞进了缓冲区
    }
    printf("sent\n");
    size = 0;

    /*
    while (size < sizeof(struct myftp_header)) {
        size_t b = recv(clientfd, &AUTH_REPLY + size, sizeof(struct myftp_header), 0);
        if (b == 0) {printf("socket Closed"); break;} // 当连接断开
        if (b < 0) {printf("Error ?"); break;} // 这里可能发生了一些意料之外的情况
        size += b; // 成功将b个byte塞进了缓冲区
    }
    */
    while((!read(clientfd, &AUTH_REPLY, sizeof(AUTH_REPLY))));
    printf("received\n");

    if (!strncmp(AUTH_REPLY.m_protocol, "\xe3myftp", 6) && (AUTH_REPLY.m_type == (char)0xA4)){
        if (AUTH_REPLY.m_status == 1){
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
    struct myftp_header LIST_REQUEST;
    char list[MAXLINE];

    //bzero(&LIST_REQUEST, sizeof(LIST_REQUEST));
    memcpy(LIST_REQUEST.m_protocol, "\xe3myftp", 6);
    LIST_REQUEST.m_type = 0xA5;
    LIST_REQUEST.m_length = 12;
    write(clientfd, &LIST_REQUEST, sizeof(LIST_REQUEST));
    //printf("waiting\n");
    while (!read(clientfd, &LIST_REQUEST, sizeof(LIST_REQUEST)));
    //printf("111\n");
    while (!read(clientfd, list, MAXLINE));
    list[LIST_REQUEST.m_length - 13] = '\0';
    //printf("222\n");
    printf("----- file list start -----\n%s----- file list end -----\n", list);
    //printf("%s", list);
 }

void download_files(char* filename){
    struct myftp_header GET_REQUEST;
    //char list[MAXLINE];
    //printf("%ld %s", strlen(filename), filename);
    //return;

    //bzero(&LIST_REQUEST, sizeof(LIST_REQUEST));
    memcpy(GET_REQUEST.m_protocol, "\xe3myftp", 6);
    GET_REQUEST.m_type = 0xA7;
    GET_REQUEST.m_length = 12 + strlen(filename) + 1;
    write(clientfd, &GET_REQUEST, sizeof(GET_REQUEST));
    write(clientfd, filename, strlen(filename));
    while (!read(clientfd, &GET_REQUEST, sizeof(GET_REQUEST)));
    if (GET_REQUEST.m_status == 1){
        while (!read(clientfd, &GET_REQUEST, sizeof(GET_REQUEST)));
        int filesize = GET_REQUEST.m_length - 12;
        char *buffer = (char*)malloc(sizeof(char)*filesize);
        while (!read(clientfd, buffer, filesize));
        FILE *fp;
        fp = fopen(filename, "wb");
        fwrite(buffer, 1, filesize, fp);
        fclose(fp);

        //printf("%s", list);
        printf("File downloaded.\n");
        
    } else{
        printf("File not existing.\n");
    }
    return;
}

void upload_files(char* filename){
    printf("File uploaded.\n");
}

void close_connection(){
    close(clientfd);
    client_status = 0;
    printf("Thank you.\n");
    return;
}