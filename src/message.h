#ifndef MESSAGE_H
#define MESSAGE_H
#include "peer.h"

int int_to_char(int i, unsigned char c[4]);
int char_to_int(unsigned char c[4]);

int create_handshake_msg(char *info_hash, char *peer_id, Peer *peer);
int create_keep_alive_msg(Peer *peer);
int create_chock_interested_msg(int type, Peer *peer);
int create_have_msg(int index,Peer *peer);
int create_bitfield_msg(char *bitfield, int bitfield_len, Peer *peer);
int create_request_msg(int index, int begin, int length, Peer *peer);
int create_piece_msg(int index, int begin ,char *block, int b_len, Peer *peer);
int create_cancel_msg(int index, int begin , int length, Peer *peer);
int create_port_msg(int port, Peer *peer);

int print_msg_buffer(unsigned char *buff, int len);
int prepare_send_have_msg();
int parse_response(Peer *peer);
int is_complete_messgae(unsigned char *buff, unsigned int len, int *ok_len);
int parse_response(Peer *peer);
int parse_response_uncomplete_msg(Peer *p, int ok_len);
int create_response_message(Peer *peer);
void discard_send_buffer(Peer *peer);


#endif // LOG_H
