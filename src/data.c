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

#define btcache_len 1024
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
    int i;
    Btcache *node, *last;

    for(i = 0; i < btcache_len; i++) {
        node = initialize_btcache_node();
        if(node == NULL) {
            printf("%s:%d create_btcache error\n", __FILE__,__LINE__);
            release_memory_in_btcache();
            return -1;
        }
        if(btcache_head == NULL) {
            btcache_head = node;
            last = node;
        } else {
            last->next = node;
            last = node;
        }
    }

    int count = file_length % piece_length / (16*1024);
    if(file_length % piece_length %(16*1024) !=0 ) count++;
    last_piece_count = count;
    last_slice_len = file_length % piece_length %(16*1024);
    if(last_slice_len == 0) last_slice_len = 16*1024;
    last_piece_index = pieces_length / 20 - 1;
    while(count > 0) {
         node = initialize_btcache_node();
         if(node == NULL) {
            printf("%s:%d create_btcache error\n", __FILE__,__LINE__);
            release_memory_in_btcache();
            return -1;
        }
        if(last_piece == NULL) {last_piece = node, last=node;}
        else {last->next = node, last=node;}

        count++;
    }

    for(i = 0; i<64;i++) {
        have_piece_index[i] = -1;
    }

    return 0;
}

void release_memory_in_btcache(){
    Btcache *p = btcache_head;
    while(p != NULL) {
        btcache_head = p->next;
        if(p->buff != NULL) free(p->buff);
        free(p);
        p = btcache_head;
    }

    release_last_piece();
    if(fds != NULL) free(fds);
}

int get_files_count(){
    int count = 0;
    if(is_multi_files() == 0) return 1;
    Files *p = files_head;

    while(p != NULL) {
        count++;
        p = p->next;
    }

    return count;
}
int create_files() {
    int ret,i;
    char buff[1] = {0x0};

    fds_len = get_files_count();

    if(fds_len < 0 ) return -1;
    fds = (int*)malloc(fds_len * sizeof(int));
    if(fds == NULL) return -1;

    if(is_multi_files() == 0) {
        *fds = open(file_name, O_RDWR|O_CREAT,0777);
        if(*fds <0) {printf("%s:%d error", __FILE__,__LINE__); return -1;}
        ret = lseek(*fds, file_length-1, SEEK_SET);
        if(ret < 0) {printf("%s:%d error", __FILE__,__LINE__); return -1;}
        ret = write(*fds, buff, 1);
        if(ret != 1) {printf("%s:%d error", __FILE__,__LINE__); return -1;}
    } else {
        ret = chdir(file_name);
        if(ret < 0) {
            ret = mkdir(file_name);
            if(ret < 0) {printf("%s:%d error", __FILE__,__LINE__); return -1;}
            ret = chdir(file_name);
            if(ret < 0) {printf("%s:%d error", __FILE__,__LINE__); return -1;}
        }
        Files *p = files_head;
        i = 0;
        while( p != NULL) {
            fds[i] = open(p->path, O_RDWR|O_CREAT, 0777);
            if(fds[i] < 0) {
                printf("%s:%d error", __FILE__,__LINE__);
                return -1;
            }

            ret = lseek(fds[i], p->length - 1,SEEK_SET);
            if(ret < 0) {
                printf("%s:%d error", __FILE__,__LINE__);
                return -1;
            }


            ret = write(fds[i],buff, 1);
            if(ret != 1) {
                printf("%s:%d error", __FILE__,__LINE__);
                return -1;
            }

            p = p->next;
            i++;
        }

    }

    return 0;
}

int write_btcache_node_to_harddisk(Btcache *node) {
    long long     line_position;
	Files         *p;
	int           i;

	if((node == NULL) || (fds == NULL))  return -1;

	// 无论是否下载多文件，将要下载的所有数据看成一个线性字节流
	// line_position指示要写入硬盘的线性位置
	// piece_length为每个piece长度，它被定义在parse_metafile.c中
	line_position = node->index * piece_length + node->begin;

	if( is_multi_files() == 0 ) {  // 如果下载的是单个文件
		lseek(*fds,line_position,SEEK_SET);
		write(*fds,node->buff,node->length);
		return 0;
	}

	// 下载的是多个文件
	if(files_head == NULL) {
		printf("%s:%d file_head is NULL",__FILE__,__LINE__);
		return -1;
	}
	p = files_head;
	i = 0;
	while(p != NULL) {
		if((line_position < p->length) && (line_position+node->length < p->length)) {
			// 待写入的数据属于同一个文件
			lseek(fds[i],line_position,SEEK_SET);
			write(fds[i],node->buff,node->length);
			break;
		}
		else if((line_position < p->length) && (line_position+node->length >= p->length)) {
			// 待写入的数据跨越了两个文件或两个以上的文件
			int offset = 0;             // buff内的偏移,也是已写的字节数
			int left   = node->length;  // 剩余要写的字节数

			lseek(fds[i],line_position,SEEK_SET);
			write(fds[i],node->buff,p->length - line_position);
			offset = p->length - line_position;        // offset存放已写的字节数
			left = left - (p->length - line_position); // 还需写在字节数
			p = p->next;                               // 用于获取下一个文件的长度
			i++;                                       // 获取下一个文件描述符

			while(left > 0)
				if(p->length >= left) {  // 当前文件的长度大于等于要写的字节数
					lseek(fds[i],0,SEEK_SET);
					write(fds[i],node->buff+offset,left); // 写入剩余要写的字节数
					left = 0;
				} else {  // 当前文件的长度小于要写的字节数
					lseek(fds[i],0,SEEK_SET);
					write(fds[i],node->buff+offset,p->length); // 写满当前文件
					offset = offset + p->length;
					left = left - p->length;
					i++;
					p = p->next;
				}

				break;
		} else {
			// 待写入的数据不应写入当前文件
			line_position = line_position - p->length;
			i++;
			p = p->next;
		}
	}
	return 0;
}

int read_slice_from_harddisk(Btcache *node) {
unsigned int  line_position;
	Files         *p;
	int           i;

	if( (node == NULL) || (fds == NULL) )  return -1;

	if( (node->index >= pieces_length/20) || (node->begin >= piece_length) ||
	    (node->length > 16*1024) )
		return -1;

	// 计算线性偏移量
	line_position = node->index * piece_length + node->begin;

	if( is_multi_files() == 0 ) {  // 如果下载的是单个文件
		lseek(*fds,line_position,SEEK_SET);
		read(*fds,node->buff,node->length);
		return 0;
	}

	// 如果下载的是多个文件
	if(files_head == NULL)  get_files_length_path();
	p = files_head;
	i = 0;
	while(p != NULL) {
		if((line_position < p->length) && (line_position+node->length < p->length)) {
			// 待读出的数据属于同一个文件
			lseek(fds[i],line_position,SEEK_SET);
			read(fds[i],node->buff,node->length);
			break;
		} else if((line_position < p->length) && (line_position+node->length >= p->length)) {
			// 待读出的数据跨越了两个文件或两个以上的文件
			int offset = 0;             // buff内的偏移,也是已读的字节数
			int left   = node->length;  // 剩余要读的字节数

			lseek(fds[i],line_position,SEEK_SET);
			read(fds[i],node->buff,p->length - line_position);
			offset = p->length - line_position;        // offset存放已读的字节数
			left = left - (p->length - line_position); // 还需读在字节数
			p = p->next;                               // 用于获取下一个文件的长度
			i++;                                       // 获取下一个文件描述符

			while(left > 0)
				if(p->length >= left) {  // 当前文件的长度大于等于要读的字节数
					lseek(fds[i],0,SEEK_SET);
					read(fds[i],node->buff+offset,left); // 读取剩余要读的字节数
					left = 0;
				} else {  // 当前文件的长度小于要读的字节数
					lseek(fds[i],0,SEEK_SET);
					read(fds[i],node->buff+offset,p->length); // 读取当前文件的所有内容
					offset = offset + p->length;
					left = left - p->length;
					i++;
					p = p->next;
				}

			break;
		} else {
			// 待读出的数据不应写入当前文件
			line_position = line_position - p->length;
			i++;
			p = p->next;
		}
	}
	return 0;
}

int write_piece_to_harddisk(int sequence, Peer *peer) {

    Btcache *node_ptr  = btcache_head, *p;
    unsigned char piece_hash1[20], piece_hash2[20];
    int slice_count = piece_length / (16*1024);
    int     index, index_copy;

    if(peer == NULL) return -1;
    int i= 0;
    while(i < sequence) {node_ptr = node_ptr->next; i++;}
    p = node_ptr;

    SHA1_CTX ctx;
    SHA1Init(&ctx);
    while( slice_count > 0 && node_ptr != NULL) {
        SHA1Update(&ctx, node_ptr->buff, 16*1024);
        slice_count--;
        node_ptr = node_ptr->next;
    }

    SHA1Final(piece_hash1, &ctx);
    index = p->index *20;
    index_copy = p->index;
    for(i=0;i<20;i++)piece_hash2[i] = pieces[index+1];
    int ret = memcmp(piece_hash1, piece_hash2, 20);
    if(ret != 0) {printf("piece hash is wrong\n"); return -1;}

    node_ptr = p;
    slice_count = piece_length / (16*1024);
    while(slice_count > 0) {
        write_btcache_node_to_harddisk(node_ptr);

        Request_piece *req_p = peer->Request_piece_head;
        Request_piece *req_q = peer->Request_piece_head;

        while(req_p != NULL) {
            if(req_p->begin == node_ptr->begin && req_p->index == node_ptr->index) {
                if(req_p == peer->Request_piece_head)
                    peer->Request_piece_head = req_p->next;
                else
                    req_q->next = req_p->next;
                free(req_p);
                req_p = req_q = NULL;
                break;
            }
            req_q = req_p;
            req_p = req_p->next;
        }

        node_ptr->index = -1;
        node_ptr->begin = -1;
        node_ptr->length = -1;
        node_ptr->in_use = 0;
        node_ptr->read_write = -1;
        node_ptr->is_full = 0;
        node_ptr->is_writed = 0;
        node_ptr->access_count = 0;
        node_ptr = node_ptr->next;
        slice_count--;
    }

    if(end_mode == 1) delete_request_end_mode(index_copy);
    set_bit_value(bitmap, index_copy,1);
    for(i = 0; i< 64;i++) {
        if(have_piece_index[i] == -1) {
            have_piece_index[i]= index_copy;
            break;
        }
    }

    download_piece_num++;
    if(download_piece_num % 10 == 0) restore_bitmap();

    printf("%%%%% Total piece download:%d %%%%%\n", download_piece_num);
    printf("writed piece index:%d", index_copy);
    return 0;

}

int read_piece_from_harddisk(Btcache *p, int index) {
    Btcache  *node_ptr   = p;
	int      begin       = 0;
	int      length      = 16*1024;
	int      slice_count = piece_length / (16*1024);
	int      ret;

	if(p==NULL || index>=pieces_length/20)  return -1;

	while(slice_count > 0) {
		node_ptr->index  = index;
		node_ptr->begin  = begin;
		node_ptr->length = length;

		ret = read_slice_from_harddisk(node_ptr);
		if(ret < 0) return -1;

		node_ptr->in_use       = 1;
		node_ptr->read_write   = 0;
		node_ptr->is_full      = 1;
		node_ptr->is_writed    = 0;
		node_ptr->access_count = 0;

		begin += 16*1024;
		slice_count--;
		node_ptr = node_ptr->next;
	}

	return 0;
}

int write_btcache_to_harddisk(Peer *peer) {
    Btcache *p  = btcache_head;
    int slice_count = piece_length / (16*1024);
    int index_count = piece_length  / (16*1024);
    int full_count = 0;
    int first_index;

    while(p != NULL) {
        if(index_count % slice_count == 0) {
            full_count = 0;
            first_index = index_count;
        }
        if(p->in_use == 1 && p->read_write == 1 && p->is_full == 1 && p->is_writed == 0) {
            full_count++;
        }
        if(full_count == slice_count) {
            write_piece_to_harddisk(first_index, peer);
        }
         index_count++;
         p = p->next;
    }

    return 0;
}

int release_read_btcache_node(int base_count) {
    Btcache *p = btcache_head;
    Btcache *q = NULL;
    int count = 0;
    int used_count = 0;
    int slice_count = piece_length / (16*1024);

    if(base_count < 0) return -1;
    while(p != NULL) {
        if(count % slice_count == 0) {used_count = 0; q = p;}
        if(p->in_use == 1 && p->read_write == 0)used_count += p->access_count;
        if(used_count == base_count) break;

        count++;
        p = p->next;
    }

    if(p != NULL) {
        p = q;

        while(slice_count > 0) {
            p->index = -1;
            p->begin  = -1;
            p->length = -1;
            p->in_use = 0;
            p->read_write = -1;
            p->is_full = 0;
            p->is_writed = 0;
            p->access_count  = 0;

            slice_count--;
            p = p->next;
        }

    }
}


// 下载完一个slice后,检查是否该slice为一个piece最后一块
// 若是则写入硬盘,只对刚刚开始下载时起作用,这样可以立即使peer得知
int is_a_complete_piece(int index, int *sequnce)
{
	Btcache          *p = btcache_head;
	int     slice_count = piece_length / (16*1024);
	int           count = 0;
	int             num = 0;
	int        complete = 0;

	while(p != NULL) {
		if( count%slice_count==0 && p->index!=index ) {
			num = slice_count;
			while(num>0 && p!=NULL)  { p = p->next; num--; count++; }
			continue;
		}
		if( count%slice_count!=0 || p->read_write!=1 || p->is_full!=1)
			break;

		*sequnce = count;
		num = slice_count;

		while(num>0 && p!=NULL) {
			if(p->index==index && p->read_write==1 && p->is_full==1)
				 complete++;
			else break;

			num--;
			p = p->next;
		}

		break;
	}

	if(complete == slice_count) return 1;
	else return 0;
}

void clear_btcache() {
    Btcache *node = btcache_head;
    while(node != NULL) {
        node->index  = -1;
        node->begin = -1;
        node->length = -1;
        node->in_use = 0;
        node->read_write =  -1;
        node->is_full = 0;
        node->is_writed = 0;
        node->access_count = 0;
        node = node->next;
    }
}

void clear_btcache_before_peer_close(Peer *peer);

int write_slice_to_btcache(int index, int begin, int length, unsigned char *buff, int len, Peer *peer) {
    int count = 0,slice_count, unuse_count;
    Btcache *p = btcache_head, *q = NULL;

    if(p == NULL) return -1;
    if(index>=pieces_length/20 || begin>piece_length-16*1024) return -1;
    if(buff == NULL || peer == NULL) return -1;
    if(index == last_piece_index) {
        write_slice_to_last_piece(index, begin, length, buff, len, peer);
    }
}
int read_slice_for_send(int index, int begin, int length ,Peer *peer);
int write_last_piece_to_btcache(Peer *peer);
int write_slice_to_last_piece(int index, int begin, int length, unsigned char *buff, int len, Peer *peer);
int read_last_piece_from_harddisk(Btcache *p, int index);
int read_slice_for_send_last_piece(int index, int begin, int length, Peer *peer);

void release_last_piece(){
    Btcache *p = last_piece;
    while(p!= NULL) {
        last_piece = p->next;
        if(p->buff != NULL) free(p->buff);
        free(p);
        p = last_piece;
    }
}
