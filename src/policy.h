#ifndef POLICY_H
#define POLICY_H
#include "peer.h"

#define COMPUTE_RETE_TIME 10        //ÿ��10������һ�θ���peer�����غ��ϴ��ٶ�
#define UNCHOKE_COUNT 4             //��������peer����
#define REQ_SLICE_NUM 4             //ÿ������slice�ĸ���

typedef struct _Unchoke_peers{
    Peer* unchkpeer[UNCHOKE_COUNT];     //���������peer��ָ��
    int count;                          //��¼�����ж��ٸ�������peer
    Peer *optunchkpeer;                 //�����Ż�������peerָ��
} Unchoke_peers;

void init_unchoke_peers();          //��ʼ��plicy.c�����ȫ�ֱ���unchoke_peers
int select_unchkoke_peer();         //ѡ��unchoke peer
int select_optunchkoke_peer();      //��pper������ѡ��һ���Ż���������peer
int compute_rate();                 //�������һ��ʱ��(10s)ÿ��peer���ϴ������ٶ�
int compute_total_rate();           //�����ܵ��ϴ������ٶ�

int is_seed(Peer *node);                //�ж�ĳ��peer�Ƿ�Ϊ����
int create_req_slice_msg(Peer *node);   //������������
int create_req_slice_msg_from_btcache(Peer *node);

#endif // POLICY_H
