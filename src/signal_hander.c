#include    <stdio.h>
#include    <unistd.h>
#include    <stdlib.h>
#include    <signal.h>
#include    "parse_metafile.h"
#include    "bitfield.h"
#include    "peer.h"
#include    "data.h"
#include    "tracker.h"
#include    "torrent.h"
#include    "signal_hander.h"

extern int download_piece_num;
extern int *fds;
extern int fds_len;
extern Peer *peer_head;

//�����˳�ʱ��ִ��һ������
void do_clear_work() {
    Peer *p = peer_head;
    //�ر�����peer��socket
    while( p!= NULL) {
        if(p->state != CLOSING) close(p->socket);
        p = p->next;
    }

    //����λͼ
    if(download_piece_num > 0) {
        restore_bitmap();
    }

    //�ر��ļ�������
    int i;
    for(i=0; i< fds_len; i++){
        close(fds[i]);
    }
    //�ͷŶ�̬������ڴ�
    release_memory_in_parse_metafile();
    release_memory_in_bitfield();
    release_memory_in_btcache();
    release_memory_in_peer();
    release_memory_in_torrent();

    exit(0);
}


void process_signal(int sinno) {
    printf("Please wait for clean operations\n");
    do_clear_work();
}

//�����źŴ�����
int set_signal_hander(){

    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("can not catch signal:sigpipe\n");
        return -1;
    }

    if(signal(SIGINT, process_signal) == SIG_ERR) {
        perror("can not catch signal:sigint\n");
        return -1;
    }

     if(signal(SIGTERM, process_signal) == SIG_ERR) {
        perror("can not catch signal:sigterm\n");
        return -1;
    }

    return 0;

}
