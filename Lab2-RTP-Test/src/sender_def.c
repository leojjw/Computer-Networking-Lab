#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/socket.h"
#include "netdb.h"
#include "arpa/inet.h"
#include "string.h"
#include "rtp.h"
#include "sender_def.h"
#include "util.h"

struct sockaddr_in recvaddr;
int sockfd_sender;
uint32_t N_sender, nextseqnum;


void sendData(uint16_t length, uint32_t seq_num, char* payload){
    struct RTP_packet packet;

    packet.rtp.type = RTP_DATA;
    packet.rtp.length = length;
    packet.rtp.seq_num = seq_num;
    packet.rtp.checksum = 0;
    memcpy(packet.payload, payload, length);
    packet.rtp.checksum = compute_checksum(&packet, sizeof(struct RTP_header) + length);

    sendto(sockfd_sender, (char*)&packet, sizeof(struct RTP_header) + length, 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr));
    return;
}

int initSender(const char* receiver_ip, uint16_t receiver_port, uint32_t window_size){
    int rand_seq_num = rand();
    struct RTP_header header;

    sockfd_sender = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    setsockopt(sockfd_sender, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    bzero(&recvaddr, sizeof(recvaddr));
    recvaddr.sin_family = AF_INET;
    inet_pton(AF_INET, receiver_ip, &recvaddr.sin_addr);
    recvaddr.sin_port = htons(receiver_port);

    header.type = RTP_START;
    header.seq_num = rand_seq_num;
    header.length = 0;
    header.checksum = 0;
    header.checksum = compute_checksum(&header, sizeof(header));
    sendto(sockfd_sender, &header, sizeof(header), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr));

    int recv = recvfrom(sockfd_sender, &header, sizeof(header), 0, NULL, NULL);
    uint32_t checksum = header.checksum;
    header.checksum = 0;
    if ((recv > 0) && (header.type == RTP_ACK) && (checksum == compute_checksum(&header, sizeof(header)))){
        N_sender = window_size;
        return 0;
    } else {
        header.type = RTP_END;
        header.length = 0;
        header.seq_num = 0;
        header.checksum = 0;
        header.checksum = compute_checksum(&header, sizeof(header));
        sendto(sockfd_sender, &header, sizeof(header), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr));
        recvfrom(sockfd_sender, &header, sizeof(header), 0, NULL, NULL);
        return -1;
    }
}

int sendMessage(const char* message){
    FILE *fp = fopen(message, "rb");
    if (fp == NULL){
        printf("File does not exist.\n");
        return -1;
    }

    struct RTP_header header;
    fseek(fp, 0, SEEK_END);
    unsigned long file_size = ftell(fp);
    uint32_t packet_num = file_size / PAYLOAD_SIZE, base = 0, checksum;
    uint32_t remainder = file_size - (packet_num * PAYLOAD_SIZE);               /* Length of the last data packet */
    ssize_t rcv;
    char *buffer = (char*)malloc(sizeof(char)*(PAYLOAD_SIZE * N_sender));
    rewind(fp);

    for (nextseqnum = 0; nextseqnum <= packet_num; nextseqnum++){
        while(1){
            if (nextseqnum < base + N_sender) {
                fread(buffer + ((nextseqnum % N_sender) * PAYLOAD_SIZE), sizeof(char), (nextseqnum == packet_num) ? remainder : PAYLOAD_SIZE, fp);
                sendData((nextseqnum == packet_num) ? remainder : PAYLOAD_SIZE, nextseqnum, buffer + ((nextseqnum % N_sender) * PAYLOAD_SIZE));
                break;
            } else {
                rcv = recvfrom(sockfd_sender, &header, sizeof(header), 0, NULL, NULL);
                checksum = header.checksum;
                header.checksum = 0;
                if (rcv == -1){
                    for (int i = base; i < nextseqnum; i++){
                        sendData((i == packet_num) ? remainder : PAYLOAD_SIZE, i, buffer + ((i % N_sender) * PAYLOAD_SIZE));
                    }
                } else if ((header.type == RTP_ACK) && (checksum == compute_checksum(&header, sizeof(header)))){
                    if (header.seq_num > base){
                        base = header.seq_num;
                    }
                }
            }
        }
    }
    fclose(fp);

    if(base < nextseqnum){
        while (1){
            rcv = recvfrom(sockfd_sender, &header, sizeof(header), 0, NULL, NULL);
            checksum = header.checksum;
            header.checksum = 0;
            if (rcv == -1){
                for (int i = base; i < nextseqnum; i++){
                    sendData((i == packet_num) ? remainder : PAYLOAD_SIZE, i, buffer + ((i % N_sender) * PAYLOAD_SIZE));
                }
            } else if ((header.type == RTP_ACK) && (checksum == compute_checksum(&header, sizeof(header)))){
                if (header.seq_num > base){
                    base = header.seq_num;
                    if (base >= nextseqnum){
                        break;
                    }
                }
            }
        }
    }
    free(buffer);

    if (nextseqnum > packet_num){
        return 0;
    } else {
        return -1;
    }
}

int sendMessageOpt(const char* message){
    FILE *fp = fopen(message, "rb");
    if (fp == NULL){
        printf("File does not exist.\n");
        return -1;
    }

    struct RTP_header header;
    fseek(fp, 0, SEEK_END);
    unsigned long file_size = ftell(fp);
    uint32_t packet_num = file_size / PAYLOAD_SIZE, base = 0, checksum;
    uint32_t remainder = file_size - (packet_num * PAYLOAD_SIZE);
    ssize_t rcv;
    char *buffer = (char*)malloc(sizeof(char)*(PAYLOAD_SIZE * N_sender));
    uint8_t* ack_num = (uint8_t*)malloc(sizeof(uint8_t)*N_sender);
    memset(ack_num, 0, N_sender);
    rewind(fp);

    for (nextseqnum = 0; nextseqnum <= packet_num; nextseqnum++){
        while(1){
            if (nextseqnum < base + N_sender) {
                fread(buffer + ((nextseqnum % N_sender) * PAYLOAD_SIZE), sizeof(char), (nextseqnum == packet_num) ? remainder : PAYLOAD_SIZE, fp);
                sendData((nextseqnum == packet_num) ? remainder : PAYLOAD_SIZE, nextseqnum, buffer + ((nextseqnum % N_sender) * PAYLOAD_SIZE));
                break;
            } else {
                rcv = recvfrom(sockfd_sender, &header, sizeof(header), 0, NULL, NULL);
                checksum = header.checksum;
                header.checksum = 0;
                if (rcv == -1){
                    for (int i = base; i < nextseqnum; i++){
                        if (!ack_num[i % N_sender])
                            sendData((i == packet_num) ? remainder : PAYLOAD_SIZE, i, buffer + ((i % N_sender) * PAYLOAD_SIZE));
                    }
                } else if ((header.type == RTP_ACK) && (checksum == compute_checksum(&header, sizeof(header)))){
                    if (header.seq_num > base && header.seq_num < (base + N_sender)){
                        ack_num[header.seq_num % N_sender] = 1;
                    } else if (header.seq_num == base){
                        base += 1;
                        for (int i = 1; i < N_sender; i++){
                            if (!ack_num[base % N_sender]){
                                break;
                            }
                            ack_num[base % N_sender] = 0;
                            base += 1;
                        }
                    }
                }
            }
        }
    }
    fclose(fp);

    if(base < nextseqnum){
        while (1){
            rcv = recvfrom(sockfd_sender, &header, sizeof(header), 0, NULL, NULL);
            checksum = header.checksum;
            header.checksum = 0;
            if (rcv == -1){
                for (int i = base; i < nextseqnum; i++){
                    if (!ack_num[i % N_sender])
                        sendData((i == packet_num) ? remainder : PAYLOAD_SIZE, i, buffer + ((i % N_sender) * PAYLOAD_SIZE));                    
                }
            } else if ((header.type == RTP_ACK) && (checksum == compute_checksum(&header, sizeof(header)))){
                if (header.seq_num > base && header.seq_num < nextseqnum){
                    ack_num[header.seq_num % N_sender] = 1;
                } else if (header.seq_num == base){
                    base += 1;
                    for (int i = 1; i < N_sender; i++){
                        if (!ack_num[base % N_sender]){
                            break;
                        }
                        ack_num[base % N_sender] = 0;
                        base += 1;
                    }
                }
                if (base == nextseqnum){
                    break;
                }
            }
        }
    }
    free(buffer);
    free(ack_num);
    
    if (nextseqnum > packet_num){
        return 0;
    } else {
        return -1;
    }
}

void terminateSender(){
    struct RTP_header header;

    header.type = RTP_END;
    header.length = 0;
    header.seq_num = nextseqnum;
    header.checksum = 0;
    header.checksum = compute_checksum(&header, sizeof(header));

    sendto(sockfd_sender, &header, sizeof(header), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr));
    recvfrom(sockfd_sender, &header, sizeof(header), 0, NULL, NULL);

    return;
}
