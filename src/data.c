#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include "parse_metafile.h"
#include "bitfield.h"
#include "message.h"
#include "sha1.h"
#include "data.h"

extern char *file_name;
extern Files *files_head;
extern int file_length;
extern int piece_length;
extern char *pieces;
extern int pieces_length;

extern Bitmap *bitmap;
extern int download_piece_num;
extern Peer *peer_head;

#define btcache_len 1024;
Btcache *btcache_head = NULL;
Btcache *last_piece = NULL;
int last_piece_index  = 0;
int last_piece_count = 0;
int last_slice_len = 0;

int *fds = NULL;
int fds_len = 0;
int have_piece_index[64];
int end_mode = 0;

Btcache *initialize_btcache_node() {
    Btcache *node;
    node = (Btcache*)malloc(sizeof(Btcache));
    if(node == NULL) return NULL;
    node->buff = (unsigned char*)malloc(16*1024);
    if(node->buff == NULL) {
        if(node != NULL)
            free(node);
        return NULL;
    }

    node->index = -1;
    node->begin = -1;
    node->in_use = 0;
    node->read_write = -1;
    node->is_full = 0;
    node->is_writed = 0;
    node->access_count = 0;
    node->next =  NULL;

    return node;
}

int create_btcache() {

}
void release_memory_in_btcache();

int get_files_count();
int create_files();

int write_btcache_node_to_harddisk(Btcache *node);
int read_slice_from_harddisk(Btcache *node);
int write_piece_to_harddisk(int sequence, Peer *peer);
int read_piece_from_harddisk(Btcache *p, int index);

int write_btcache_to_harddisk(Peer *peer);
int release_read_btcache_node(int base_count);
void clear_btcache_before_peer_close(Peer *peer);
int write_slice_to_btcache(int index, int begin, int length, unsigned char *buff, int len, Peer *peer);
int read_slice_for_send(int index, int begin, int length ,Peer *peer);
int write_last_piece_to_btcache(Peer *peer);
int write_slice_to_last_piece(int index, int begin, int length, unsi char *buff, int len, Peer *peer);
int read_last_piece_from_harddisk(Btcache *p, int index);
int read_slice_for_send_last_piece(int index, int begin, int length, Peer *peer);
void release_last_piece();
