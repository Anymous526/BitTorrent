#ifndef DATA_H
#define DATA_H
#include "peer.h"

//每个btcache结点维护一个长度为16kb的缓冲区,该缓冲保存一个slice的数据
typedef struct _Btcache{
    unsigned char *buff;        //指向缓冲区的指针
    int     index;              //数据所有的piece块的索引
    int     begin;              //数据在piece块中的起始位置
    int     length;             //数据的长度

    unsigned char in_use;        //该缓冲区是否在使用中
    unsigned char read_write;   //是发送给peer的数据还是接收的数据
                                //若数据从硬盘读出,read write为0
                                //若数据将要写入硬盘,red write 为1

    unsigned char is_full;      //该缓冲是否满
    unsigned char is_writed;    //缓冲区中的数据是否已经写入到硬盘中
    int     access_count;       //对该缓冲区的访问计数
    struct _Btcache *next;
}Btcache;

Btcache *initialize_btcache_node();     //为btcache结点分配内存穴并进行初始化
int create_btcache();                   //他问总大小为 16k * 1014即16M的缓冲区
void release_memory_in_btcache();       //释放data.c中动态分的内存

int get_files_count();                  //获取种子文件中待下载的文件个数
int create_files();                     //根据种子文件中的信息创建保存下载数据的文件

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
int write_slice_to_last_piece(int index, int begin, int length, unsigned char *buff, int len, Peer *peer);
int read_last_piece_from_harddisk(Btcache *p, int index);
int read_slice_for_send_last_piece(int index, int begin, int length, Peer *peer);
void release_last_piece();
#endif
