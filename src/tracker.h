#ifndef TRACKER_H
#define TRACKER_H
#include <netinet/in.h>
#include "parse_metafile.h"

typedef struct _Peer_addr{
    char    ip[16];
    unsigned short port;
    struct _Peer_addr *next;
}Peer_addr;

//���ڽ�info_hash��peer_idת����http�����ʽ
int http_encode(unsigned char *in, int len1, char *out, int len2);
//�������ļ��д洢��Tracker��URL��ȡTracker������
int get_tracker_name(Announce_list *node, char *name, int len);
//�������ļ��д洢��Tracker��URL��ȡTracker�˿ں�
int get_tracker_port(Announce_list *node, unsigned short *port);

//���췢�͵�Tracker��������HTTP GET����
int create_request(char *request, int len, Announce_list *node,
                   unsigned short port,  long long down, long long up,
                   long long left, int numwant);

int prepare_connect_tracker(int *max_sockfd);  //�Է������ķ�ʽ����Tracker
int prepare_connect_peer(int *max_sockfd);      //�Է������ķ�ʽ����peer

//��ȡTracker���ص���Ϣ����
int get_response_type(char *buffer, int len, int *total_length);
//������һ��Tracker���ص���Ϣ
int parse_tracker_response1(char *buffer, int ret, char *redirection, int len);
//�����ڶ���Tracker���ص���Ϣ
int parse_tracker_response2(char *buffer, int ret);
//Ϊ�ѽ��������ӵ�peer����peer��㲢���뵽peer������
int add_peer_node_to_peerlist(int *sock, struct sockaddr_in saptr);
//�ͷ�peer_addrr����
void free_peer_addr_head();

#endif // TRACKER_H
