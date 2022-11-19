#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/socket.h"
#include "netdb.h"
#include "arpa/inet.h"
#include "string.h"
#include "receiver_def.h"
#include "rtp.h"
#include "util.h"

struct sockaddr_in sendaddr;
int sockfd_receiver;
uint32_t N_receiver;

void sendACK(uint32_t seq_num){
    struct RTP_header header;

    header.type = RTP_ACK;
    header.length = 0;
    header.seq_num = seq_num;
    header.checksum = 0;
    header.checksum = compute_checksum(&header, sizeof(header));

    sendto(sockfd_receiver, &header, sizeof(header), 0, (struct sockaddr *)&sendaddr, sizeof(sendaddr));
}

int initReceiver(uint16_t port, uint32_t window_size){
    struct sockaddr_in recvaddr;
    struct RTP_header header;
    int len = sizeof(sendaddr);

    sockfd_receiver = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(sockfd_receiver, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    bzero(&recvaddr, sizeof(recvaddr));
    recvaddr.sin_family = AF_INET;
    recvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    recvaddr.sin_port = htons(port);
    bind(sockfd_receiver, (struct sockaddr *)&recvaddr, sizeof(recvaddr));

    recvfrom(sockfd_receiver, &header, sizeof(header), 0, (struct sockaddr *)&sendaddr, &len);
    uint32_t checksum = header.checksum;
    header.checksum = 0;
    if ((header.type == RTP_START) && (checksum == compute_checksum(&header, sizeof(header)))){
        header.type = RTP_ACK;
        header.checksum = compute_checksum(&header, sizeof(header));
        sendto(sockfd_receiver, &header, sizeof(header), 0, (struct sockaddr *)&sendaddr, sizeof(sendaddr));
        N_receiver = window_size;
        return 0;
    }
    return -1;
}

int recvMessage(char* filename){
    FILE *fp = fopen(filename, "wb");

    struct RTP_packet packet;
    uint32_t expectseqnum = 0, rcvdseqnum, checksum;
    int received_bytes = 0;
    uint8_t* ack_num = (uint8_t*)malloc(sizeof(uint8_t)*(N_receiver-1));
    memset(ack_num, 0, N_receiver);
    
    char *buffer = (char*)malloc(sizeof(char)*(PAYLOAD_SIZE * (N_receiver-1)));
    int *buffer_length = (int*)malloc(sizeof(int)*(N_receiver-1));
    ssize_t ret, len, b;

    while(1){
        ret = recvfrom(sockfd_receiver, &packet, sizeof(packet), 0, NULL, NULL);
        rcvdseqnum = packet.rtp.seq_num;
        checksum = packet.rtp.checksum;
        packet.rtp.checksum = 0;
        if (packet.rtp.type == RTP_DATA){
            ret -= sizeof(struct RTP_header);
            len = packet.rtp.length;
            if (checksum == compute_checksum(&packet, sizeof(struct RTP_header) + len)){
                if (rcvdseqnum == expectseqnum){
                    received_bytes += ret;
                    fwrite(packet.payload, sizeof(char), len, fp);
                    expectseqnum += 1;
                    for (int i = 1; i < N_receiver; i++){
                        if (ack_num[expectseqnum % (N_receiver-1)]){
                            ack_num[expectseqnum % (N_receiver-1)] = 0;
                            fwrite(buffer + ((expectseqnum % (N_receiver-1)) * PAYLOAD_SIZE), sizeof(char), buffer_length[expectseqnum % (N_receiver-1)], fp);
                        } else {
                            break;
                        }
                        expectseqnum += 1;
                    }
                    sendACK(expectseqnum);
                } else if (rcvdseqnum > expectseqnum && rcvdseqnum < (expectseqnum + N_receiver)) {
                    if (!ack_num[rcvdseqnum % (N_receiver-1)]){
                        memcpy(buffer + ((rcvdseqnum % (N_receiver-1)) * PAYLOAD_SIZE), packet.payload, len);
                        received_bytes += ret;
                        ack_num[rcvdseqnum % (N_receiver-1)] = 1;
                        buffer_length[rcvdseqnum % (N_receiver-1)] = len;
                        sendACK(expectseqnum);
                    }
                } else if (rcvdseqnum < expectseqnum){
                    sendACK(expectseqnum);
                }
            }
        } else if ((packet.rtp.type == RTP_END) && (checksum == compute_checksum(&packet, sizeof(struct RTP_header)))) {
            sendACK(packet.rtp.seq_num);
            close(sockfd_receiver);
            fclose(fp);
            free(ack_num);
            free(buffer);
            free(buffer_length);
            return received_bytes;
        } else if (ret == -1){
            break;
        }
    }
    fclose(fp);
    free(ack_num);
    free(buffer);
    free(buffer_length);
    return -1;
}

int recvMessageOpt(char* filename){
    FILE *fp = fopen(filename, "wb");

    struct RTP_packet packet;
    uint32_t expectseqnum = 0, rcvdseqnum, checksum;
    int received_bytes = 0;
    uint8_t* ack_num = (uint8_t*)malloc(sizeof(uint8_t)*(N_receiver-1));
    memset(ack_num, 0, N_receiver-1);
    
    char *buffer = (char*)malloc(sizeof(char)*(PAYLOAD_SIZE * (N_receiver-1)));
    int *buffer_length = (int*)malloc(sizeof(int)*(N_receiver-1)), idx;
    ssize_t ret, len;

    while(1){
        ret = recvfrom(sockfd_receiver, &packet, sizeof(packet), 0, NULL, NULL);
        rcvdseqnum = packet.rtp.seq_num;
        checksum = packet.rtp.checksum;
        packet.rtp.checksum = 0;
        if (packet.rtp.type == RTP_DATA){
            ret -= sizeof(struct RTP_header);
            len = packet.rtp.length;
            if (checksum == compute_checksum(&packet, sizeof(struct RTP_header) + len)){
                if (rcvdseqnum == expectseqnum){
                    received_bytes += ret;
                    fwrite(packet.payload, sizeof(char), len, fp);
                    sendACK(rcvdseqnum);
                    expectseqnum += 1;
                    for (int i = 1; i < N_receiver; i++){
                        idx = expectseqnum % (N_receiver-1);
                        if (ack_num[idx]){
                            ack_num[idx] = 0;
                            fwrite(buffer + (idx * PAYLOAD_SIZE), sizeof(char), buffer_length[idx], fp);
                        } else {
                            break;
                        }
                        expectseqnum += 1;
                    }
                } else if (rcvdseqnum > expectseqnum && rcvdseqnum < (expectseqnum + N_receiver)) {
                    idx = rcvdseqnum % (N_receiver-1);
                    if (!ack_num[idx]){
                        memcpy(buffer + (idx * PAYLOAD_SIZE), packet.payload, len);
                        received_bytes += ret;
                        ack_num[idx] = 1;
                        buffer_length[idx] = len;
                        sendACK(rcvdseqnum);
                    }
                } else if (rcvdseqnum < expectseqnum){
                    sendACK(rcvdseqnum);
                }
            }
        } else if ((packet.rtp.type == RTP_END) && (checksum == compute_checksum(&packet, sizeof(struct RTP_header)))) {
            sendACK(packet.rtp.seq_num);
            close(sockfd_receiver);
            fclose(fp);
            free(ack_num);
            free(buffer);
            free(buffer_length);
            return received_bytes;
        } else if (ret == -1){
            break;
        }
    } 
    fclose(fp);
    free(ack_num);
    free(buffer);
    free(buffer_length);
    return -1;
}

void terminateReceiver(){
    close(sockfd_receiver);
    return;
}
