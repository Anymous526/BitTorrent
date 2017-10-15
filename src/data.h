#ifndef DATA_H
#define DATA_H
#include "peer.h"

//ÿ��btcache���ά��һ������Ϊ16kb�Ļ�����,�û��屣��һ��slice������
typedef struct _Btcache{
    unsigned char *buff;        //ָ�򻺳�����ָ��
    int     index;              //�������е�piece�������
    int     begin;              //������piece���е���ʼλ��
    int     length;             //���ݵĳ���

    unsigned char in_use;        //�û������Ƿ���ʹ����
    unsigned char read_write;   //�Ƿ��͸�peer�����ݻ��ǽ��յ�����
                                //�����ݴ�Ӳ�̶���,read writeΪ0
                                //�����ݽ�Ҫд��Ӳ��,red write Ϊ1

    unsigned char is_full;      //�û����Ƿ���
    unsigned char is_writed;    //�������е������Ƿ��Ѿ�д�뵽Ӳ����
    int     access_count;       //�Ըû������ķ��ʼ���
    struct _Btcache *next;
}Btcache;

Btcache *initialize_btcache_node();     //Ϊbtcache�������ڴ�Ѩ�����г�ʼ��
int create_btcache();                   //�����ܴ�СΪ 16k * 1014��16M�Ļ�����
void release_memory_in_btcache();       //�ͷ�data.c�ж�̬�ֵ��ڴ�

int get_files_count();                  //��ȡ�����ļ��д����ص��ļ�����
int create_files();                     //���������ļ��е���Ϣ���������������ݵ��ļ�

//�ж�һ��Btcache����е�����Ҫд���ĸ� �ļ��Լ������λ��,��д��
int write_btcache_node_to_harddisk(Btcache *node);
//��Ӳ�̶���һ��slice�����ݴ�ŵ�������,��peer��Ҫʱ���͸�peer
//Ҫ�����slice������,indx,begin,legnth�Ѵ浽node��ָ��Ľ����
int read_slice_from_harddisk(Btcache *node);
//���һ��piece�������Ƿ���ȷ,����ȷ�򶨸�Ӳ���ϵ��ļ�
int write_piece_to_harddisk(int sequence, Peer *peer);
//��Ӳ���ϵ��ļ��ж�ȡһ��piece��ŵ�pָ����ָ��Ļ�������
int read_piece_from_harddisk(Btcache *p, int index);

//�����������������ص����ݶ��嵽Ӳ���ϵ��ļ���
int write_btcache_to_harddisk(Peer *peer);
//��������������ʱ,�ͷ���Щ��Ӳ���϶�ȡ��piece
int release_read_btcache_node(int base_count);
//��btcache�����������Щδ������ص�piece
void clear_btcache_before_peer_close(Peer *peer);
//���ոմ�per����ȡ��һ��slice��ŵ�������
int write_slice_to_btcache(int index, int begin, int length, unsigned char *buff, int len, Peer *peer);
//����������ȡһ��slice,��ȡ��slice��ŵ�peer�ķ��ͻ�����
int read_slice_for_send(int index, int begin, int length ,Peer *peer);

//����Ϊ���غ��ϴ����һ��piece�����ӵĺ���
//���һ��piece��Ϊ����,��Ϊ����һ����������piece
int write_last_piece_to_btcache(Peer *peer);
int write_slice_to_last_piece(int index, int begin, int length, unsigned char *buff, int len, Peer *peer);
int read_last_piece_from_harddisk(Btcache *p, int index);
int read_slice_for_send_last_piece(int index, int begin, int length, Peer *peer);
void release_last_piece();

#endif
