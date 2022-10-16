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
void authentication(char* username, char* password);
void list_files();
void download_files(char* filename);
void upload_files(char* filename);
void close_connection();

void check_connection();

int main(int argc, char **argv)
{
    //printf("%ld\n", sizeof(struct myftp_header_auth));
    char *host, *port, *payload, *p2, buf[MAXLINE];

    char delim[] = " ";
    
    while (fgets(buf, MAXLINE, stdin) != NULL) {
        //printf("%ld", strlen(buf));
        //buf[strlen(buf) - 1] = ' ';
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
            //printf("auth come\n");
            if (client_status == 0){
                printf("No connection existing.\n");
                continue;
            }
            if (client_status == 2){
                printf("Authentication already granted.\n");
                continue;
            }
            payload = strtok(NULL, delim);
            p2 = strtok(NULL, delim);
            //strtok(NULL, delim);
            //password = strtok(NULL, delim);
            //printf("%ld %ld\n", strlen(username), strlen(password));
            authentication(payload, p2);
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
    struct myftp_header yoyo;

    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, host, &servaddr.sin_addr);
    if (!connect(clientfd, (struct sockaddr *) &servaddr, sizeof(servaddr))){
        //bzero(&OPEN_CONN_REQEST, sizeof(OPEN_CONN_REQEST));
        memcpy(yoyo.m_protocol, "\xe3myftp", 6);
        yoyo.m_type = 0xA1;
        yoyo.m_length = htonl(12);
        write(clientfd, &yoyo, sizeof(yoyo));
        while (!read(clientfd, &yoyo, sizeof(yoyo)));
        //printf("yo %s\n", OPEN_CONN_REQEST.m_protocol);
        if (yoyo.m_status == 1){
            client_status= 1;
            printf("Server connection accepted.\n");
            return;
        }
    }
    printf("connection error\n");
    return;
 }

 void authentication(char* payload, char* p2){
    struct myFTP_message{
        struct myftp_header AUTH_REQUEST;
        char payload[50];
    } AUTH_MESSAGE;
    struct myftp_header AUTH_REPLY;
    //struct myftp_header_auth AUTH_REQUEST;
    char send_buffer[MAXLINE];
    char payl[MAXLINE] = "";
    memset(send_buffer, 0, sizeof(send_buffer));

    //int i = strlen(payload) + 1;
    //payload[strlen(payload)] = ' ';

    //printf("%ld %s\n", strlen(payload), payload);

    //memcpy(AUTH_REQUEST.m_payload, payload, 12);
    //AUTH_REQUEST.m_payload[11] = '\0';

    int i = 0;
    while (p2[i] > 32 && p2[i] < 127)
        i++;
    p2[i] = '\0';
    //printf("%ld %ld\n", strlen(payload), strlen(p2));
    int len = strlen(payload) + strlen(p2);
    //printf("%d\n", len);
    strcat(payl, payload);
    payl[strlen(payload)] = ' ';
    strcat(payl, p2);
    //printf("%s\n", payl);


    //strcpy(AUTH_REQUEST.m_payload, payload);
    //printf("%ld %s\n", strlen(AUTH_REQUEST.m_payload), AUTH_REQUEST.m_payload);

    //strcat(username, " ");
    //strcat(username, password);
    //printf("%ld %s yoyo\n", strlen(username), username);
    //printf("payload come\n");

    //bzero(&AUTH_REQUEST, sizeof(AUTH_REQUEST));
    memcpy(AUTH_MESSAGE.AUTH_REQUEST.m_protocol, "\xe3myftp", 6);
    AUTH_MESSAGE.AUTH_REQUEST.m_type = 0xA3;
    //printf("1 %x\n", AUTH_REQUEST.m_type);
    AUTH_MESSAGE.AUTH_REQUEST.m_length = htonl(12 + len + 2);
    //printf("%d\n", (AUTH_REQUEST.m_length));
    //memcpy(send_buffer, &AUTH_REQUEST, sizeof(AUTH_REQUEST));
    memcpy(AUTH_MESSAGE.payload, payl, len + 2);
    AUTH_MESSAGE.payload[len + 2 - 1] = '\0';
    //printf("2 %x\n", AUTH_REQUEST.m_type);
    send(clientfd, &AUTH_MESSAGE, 12 + len + 2, 0);
    //write(clientfd, payload, strlen(payload));
    //printf("%ld\n", strlen(payload));
    //printf("writed\n");
    //while (!read(clientfd, &AUTH_REQUEST, sizeof(AUTH_REQUEST)));
    while (!recv(clientfd, &AUTH_REPLY, 12, 0));
    //memcpy(&AUTH_REQUES, send_buffer, sizeof(struct myftp_header));
    //printf("waiting over\n");
    if (!strncmp(AUTH_REPLY.m_protocol, "\xe3myftp", 6) && AUTH_REPLY.m_type == (char)0xA4){
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