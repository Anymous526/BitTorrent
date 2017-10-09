#ifndef POLICY_H
#define POLICY_H

#define COMPUTE_RETE_TIME 10
#define UNCHOKE_COUNT 4
#define REQ_SLICE_NUM 4

typedef struct _Unchoke_peers{
    Peer* unchkpeer[UNCHOKE_COUNT];
    int count;
    Peer * optunchkpeer;
} Unchoke_peers;

void init_unchoke_peers();
int select_unchkoke_peer();
int select_optunchkoke_peer();
int compute_rate();
int compute_total_rate();

int is_seed(Peer *node);
int create_req_slice_msg(Peer *node);

#endif // POLICY_H
