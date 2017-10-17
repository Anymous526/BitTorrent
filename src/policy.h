#ifndef POLICY_H
#define POLICY_H
#include "peer.h"

#define COMPUTE_RETE_TIME 10        //每隔10抄计算一次各个peer的下载和上传速度
#define UNCHOKE_COUNT 4             //非阻塞的peer个数
#define REQ_SLICE_NUM 4             //每次请求slice的个数

typedef struct _Unchoke_peers{
    Peer* unchkpeer[UNCHOKE_COUNT];     //保存非阻塞peer的指针
    int count;                          //记录当明有多少个非阻塞peer
    Peer *optunchkpeer;                 //可在优化非阻塞peer指针
} Unchoke_peers;

void init_unchoke_peers();          //初始化plicy.c定义的全局变量unchoke_peers
int select_unchkoke_peer();         //选择unchoke peer
int select_optunchkoke_peer();      //从pper队列中选择一个优化非阻塞的peer
int compute_rate();                 //计算最近一段时间(10s)每个peer的上传下载速度
int compute_total_rate();           //计算总的上传下载速度

int is_seed(Peer *node);                //判断某个peer是否为种子
int create_req_slice_msg(Peer *node);   //构造数据请求
int create_req_slice_msg_from_btcache(Peer *node);

#endif // POLICY_H
