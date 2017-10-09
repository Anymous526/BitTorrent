#ifndef PEER_H
#define PEER_H
#include <string.h>
#include <time.h>
#include "bitfield.h"

#define INITIAL         -1
#define HALFSHAKED      0
#define HANDSHAKED      1
#define SEDNBITFIELD    2
#define RECVBITFIELD    3
#define DATA            4
#define CLOSING         5

#define MSG_SIZE (22*1024 + 16*1024)

typedef struct _Request_piece {
    int index;
    int begin;
    int length;
    struct _Request_piece *next;
}Request_piece;

typedef struct _Peer{
    int             socket;
    char            ip[16];
    unsigned short  port;
    char            id[21];

    int     state;
    int     am_choking;
    int     am_interested;
    int     peer_choking;
    int     peer_interested;

    Bitmap  bitmap;

    char    *in_buff;
    int     buff_len;
    char    *out_msg;
    int     msg_len;
    char    *out_msg_copy;
    int     msg_copy_len;
    int     msg_copy_index;

    Request_piece *Request_piece_head;
    Request_piece *Requested_piece_head;

    unsigned int down_total;
    unsigned int up_total;

    time_t      start_timestamp;
    time_t      recet_timestamp;
    time_t      last_down_timestamp;
    time_t      last_up_timestamp;
    long long   down_count;
    long long   up_count;
    float       down_state;
    float       up_state;

    struct _Peer  *next;

} Peer;

int initialize_peer(Peer *peer);
Peer*   add_peer_node();
int del_peer_node(Peer *peer);
void free_peer_node(Peer *node);
int cancel_request_list(Peer *node);
int cancel_requested_list(Peer *node);
void release_memory_in_peer();
void print_peers_data();

#endif // LOG_H
