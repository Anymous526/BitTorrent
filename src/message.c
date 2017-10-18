#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "parse_metafile.h"
#include "bitfield.h"
#include "peer.h"
#include "policy.h"
#include "data.h"
#include "message.h"

#define     HANDSHAKE       -2      //握手消息
#define     KEEP_ALIVE      -1      //keep alive 消息
#define     CHOKE           0       //choke消息
#define     UNCHOKE         1       //unchoke 消息
#define     INTERESTED      2       //interested 消息
#define     UNINTERESTED    3       //uninterested消息
#define     HAVE            4       //hava消息
#define     BITFIELD        5       //bitfield消息
#define     REQUEST         6       //request 消息
#define     PIECE           7       //piece消息
#define     CANCEL          8       //cancel消息
#define     PORT            9       //port消息

//如果45秒未给某peer 发送消息,则发送keep_alive消息
#define     KEEP_ALIVE_TIME 45

extern Bitmap   *bitmap;            //在bitmap.c中定义, 指向已方的位图
extern char     info_hash[20];      //在parse_meatfile.c中定义, 存放info_hash
extern char     peer_id[20];        //在parse_metafile.c中定义,存放peer_id
extern char     have_piece_index[64];   //在data.c中定义,存放下载的piece的index
extern Peer     *peer_head;         //在peer.c中定义,指向peer链表

int int_to_char(int i, unsigned char c[4]) {
    c[3] = i%256;
    c[2] = (i-c[3])/256%256;
    c[1] = (i-c[3]-c[2]*256)/256/256%256;
    c[0] = (i-c[3]-c[2]*256-c[1]*256*256)/256/256/256%256;

    return 0;
}

int char_to_int(unsigned char c[4]) {
    int i;
    i = c[0]*256*256*256 + c[1]*256*256 + c[2]*256 + c[3];
    return i;
}

int create_handshake_msg(char *info_hash, char *peer_id, Peer *peer) {
    // int i;
    unsigned char keyword[20] = "BitTorrent protocol", c = 0x00;
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;

    if(len < 68) return -1; //握手消息的长度为固定为68 字节

    buffer[0] = 19;
    for(int i = 0; i < 19; i++) buffer[i+1] = keyword[i];
    for(int i = 0; i < 8;  i++) buffer[i+20] = c;
    for(int i = 0; i < 20; i++) buffer[i+28] = info_hash[i];
    for(int i = 0; i < 20; i++) buffer[i+48] = peer_id[i];

    peer->msg_len += 68;

    return 0;
}

int create_keep_alive_msg(Peer *peer){

    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;

    if(len < 4) return -1;  //keep_alive消息的长度为固定的4字节
    memset(buffer,0,4);
    peer->msg_len +=4;
    return 0;
}

int create_chock_interested_msg(int type, Peer *peer) {

    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;

    //choke, unchoke,interested, uninterested消息的长度为固定 5字节
    if(len < 5) return -1;
    memset(buffer, 0,5);
    buffer[3] = 1;
    buffer[4] = type;

    peer->msg_len += 5;
    return 0;
}

int create_have_msg(int index,Peer *peer) {

    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;
    unsigned char c[4];

    if(len < 9) return -1;      //hava消息的长度固定为 9字节
    memset(buffer,0,9);
    buffer[3] = 5;
    buffer[4] = 4;
    int_to_char(index, c);  //index为piece的下标

    buffer[5] = c[0];
    buffer[6] = c[1];
    buffer[7] = c[2];
    buffer[8] = c[3];

    peer->msg_len += 9;

    return 0;

}

int create_bitfield_msg(char *bitfield, int bitfield_len, Peer *peer){
    //int i;
    unsigned char c[4];
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;

    if(len < bitfield_len + 5) { //bitfield消息的长度为bitfield_len + 5
        printf("%s:%d buffer to small\n", __FILE__, __LINE__);
        return -1;
    }
    int_to_char(bitfield_len+1, c); //位置消息的负载长度为位图长度加1
    for(int i = 0; i < 4; i++) buffer[i] = c[i];
    buffer[4] = 5;
    for(int i = 0; i < bitfield_len; i++) buffer[i+5] = bitfield[i];

    peer->msg_len += bitfield_len + 5;
    return 0;
}

int create_request_msg(int index, int begin, int length, Peer *peer){
    //int i;
    unsigned char c[4];
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;

    if(len < 17) return -1; //request消息的长度为 17
    memset(buffer, 0 ,17);
    buffer[3] = 13;
    buffer[4] = 6;
    int_to_char(index, c);
    for(int i = 0; i < 4; i++) buffer[i+5] = c[i];
    int_to_char(begin, c);
    for(int i = 0; i < 4; i++) buffer[i+9] = c[i];
    int_to_char(length, c);
    for(int i=0; i<4; i++) buffer[i+13] = c[i];

    peer->msg_len += 17;
    return 0;
}

int create_piece_msg(int index, int begin ,char *block, int b_len, Peer *peer){
    //int i;
    unsigned char c[4];
    unsigned char *buffer = peer->out_msg + peer->msg_len;
    int len = MSG_SIZE - peer->msg_len;

    if(len < b_len + 13 ){  //piece消息的长度为 b_len + 13
        printf("%s:%d buffer to small\n",__FILE__, __LINE__);
        return -1;
    }
    int_to_char(b_len+19, c);
    for(int i = 0; i < 4; i++) buffer[i]= c[i];
    buffer[4] = 7;
    int_to_char(index, c);
    for(int i = 0; i < 4; i++) buffer[i+5] = c[i];
    int_to_char(begin, c);
    for(int i = 0; i < 4;i++) buffer[i+9] = c[i];
    for(int i = 0; i < b_len;i++) buffer[i+13] = block[i];

    peer->msg_len += b_len + 13;
    return 0;
}

int create_cancel_msg(int index, int begin , int length, Peer *peer){
    //int            i;
	unsigned char  c[4];
	unsigned char  *buffer = peer->out_msg + peer->msg_len;
	int            len = MSG_SIZE - peer->msg_len;

	if(len < 17)  return -1;  // 17为cancel消息的固定长度

	memset(buffer,0,17);
	buffer[3] = 13;
	buffer[4] = 8;
	int_to_char(index,c);
	for(int i = 0; i < 4; i++)  buffer[i+5]  = c[i];
	int_to_char(begin,c);
	for(int i = 0; i < 4; i++)  buffer[i+9]  = c[i];
	int_to_char(length,c);
	for(int i = 0; i < 4; i++)  buffer[i+13] = c[i];

	peer->msg_len += 17;
	return 0;
}

int create_port_msg(int port, Peer *peer) {
    unsigned char  c[4];
	unsigned char  *buffer = peer->out_msg + peer->msg_len;
	int            len = MSG_SIZE - peer->msg_len;

	if( len < 7)  return 0;  // port消息的固定长度 7

	memset(buffer,0,7);
	buffer[3] = 3;
	buffer[4] = 9;
	int_to_char(port,c);
	buffer[5] = c[2];
	buffer[6] = c[3];

	peer->msg_len += 7;
	return 0;
}

int is_complete_messgae(unsigned char *buff, unsigned int len, int *ok_len) {

    unsigned int   i;
	char           btkeyword[20];

	unsigned char  keep_alive[4]   = { 0x0, 0x0, 0x0, 0x0 };
	unsigned char  chocke[5]       = { 0x0, 0x0, 0x0, 0x1, 0x0};
	unsigned char  unchocke[5]     = { 0x0, 0x0, 0x0, 0x1, 0x1};
	unsigned char  interested[5]   = { 0x0, 0x0, 0x0, 0x1, 0x2};
	unsigned char  uninterested[5] = { 0x0, 0x0, 0x0, 0x1, 0x3};
	unsigned char  have[5]         = { 0x0, 0x0, 0x0, 0x5, 0x4};
	unsigned char  request[5]      = { 0x0, 0x0, 0x0, 0xd, 0x6};
	unsigned char  cancel[5]       = { 0x0, 0x0, 0x0, 0xd, 0x8};
	unsigned char  port[5]         = { 0x0, 0x0, 0x0, 0x3, 0x9};

	if(buff==NULL || len<=0 || ok_len==NULL)  return -1;
	*ok_len = 0;

	btkeyword[0] = 19;
	memcpy(&btkeyword[1],"BitTorrent protocol",19);  // BitTorrent协议关键字

	unsigned char  c[4];
	unsigned int   length;

	for(i = 0; i < len; ) {
		// 握手、chocke、have等消息的长度是固定的
		if( i+68<=len && memcmp(&buff[i],btkeyword,20)==0 )         i += 68;
		else if( i+4 <=len && memcmp(&buff[i],keep_alive,4)==0 )    i += 4;
		else if( i+5 <=len && memcmp(&buff[i],chocke,5)==0 )        i += 5;
		else if( i+5 <=len && memcmp(&buff[i],unchocke,5)==0 )      i += 5;
		else if( i+5 <=len && memcmp(&buff[i],interested,5)==0 )    i += 5;
		else if( i+5 <=len && memcmp(&buff[i],uninterested,5)==0 )  i += 5;
		else if( i+9 <=len && memcmp(&buff[i],have,5)==0 )          i += 9;
		else if( i+17<=len && memcmp(&buff[i],request,5)==0 )       i += 17;
		else if( i+17<=len && memcmp(&buff[i],cancel,5)==0 )        i += 17;
		else if( i+7 <=len && memcmp(&buff[i],port,5)==0 )          i += 7;
		else if( i+5 <=len && buff[i+4]==5 )  { 	// bitfield消息的长度是变化的
			c[0] = buff[i];   c[1] = buff[i+1];
			c[2] = buff[i+2]; c[3] = buff[i+3];
			length = char_to_int(c);
			// 消息长度占4字节,消息本身占length个字节
			if( i+4+length <= len )  i += 4+length;
			else { *ok_len = i; return -1; }
		} else if( i+5 <=len && buff[i+4]==7 )  { // piece消息的长度也是变化的
			c[0] = buff[i];   c[1] = buff[i+1];
			c[2] = buff[i+2]; c[3] = buff[i+3];
			length = char_to_int(c);
			// 消息长度占4字节,消息本身占length个字节
			if( i+4+length <= len )  i += 4+length;
			else { *ok_len = i; return -1; }
		} else { // 处理未知类型的消息

			if(i+4 <= len) {
				c[0] = buff[i];   c[1] = buff[i+1];
				c[2] = buff[i+2]; c[3] = buff[i+3];
				length = char_to_int(c);
				// 消息长度占4字节,消息本身占length个字节
				if(i+4+length <= len)  { i += 4+length; continue; }
				else { *ok_len = i; return -1; }
			}
			// 如果也不是未知消息类型,则认为目前接收的数据还不是一个完整的消息
			*ok_len = i;
			return -1;
		}
	}

	*ok_len = i;
	return 1;
}

int process_handshake_msg(Peer *peer, unsigned char *buff, int len) {

    if(peer = NULL || buff == NULL) return -1;
    if(memcmp(info_hash, buff+28,20) != 0) { //若info_hash不一致则关闭
        peer->state = CLOSING;
        //丢弃发送缓冲区中的数据
        discard_send_buffer(peer);
        clear_btcache_before_peer_close(peer);
        close(peer->socket);
        return -1;
    }
    //保存该peer的peer_id
    memcmp(peer->id, buff+48, 20);
    (peer->id)[20] = '\0';
    if(peer->state == INITIAL)  {
        create_handshake_msg(info_hash, peer_id, peer);
        peer->state = HANDSHAKE;
    }
    //若握手消息已经发送, 则状态转换为已握手状态
    if(peer->state == HALFSHAKED) peer->state = HANDSHAKE;
    //记录最近收到该peer消息的时间
    //若一定时间内容(如两分钟)未收到该peer的任何消息,则关闭连接
    peer->start_timestamp = time(NULL);
    return 0;
}

int process_keep_alive_msg(Peer *peer, unsigned char *buff, int len) {

    if(peer = NULL || buff == NULL) return -1;
    //若记录最近收到该pper消息的时候
    //若一定时间内(如2min)未收到该peer的任何消息,则关闭连接
    peer->start_timestamp = time(NULL);
    return 0;
}

int process_choke_msg(Peer *peer, unsigned char *buff, int len) {

    if(peer == NULL || buff == NULL) return -1;
    if(peer->state != CLOSING  && peer->peer_choking == 0) {

        peer->peer_choking = 1;
        peer->last_down_timestamp = 0;  //将最近接收到来自该peer数据的时间清零
        peer->down_count =0;            //将最近从pper处下载的字节数清零
        peer->down_rate = 0;            //将最近从pper下载数据的速度清零
    }

    peer->start_timestamp = time(NULL);
    return 0;
}

int process_unchoke_msg(Peer *peer, unsigned char *buff, int len) {

    if(peer == NULL || buff == NULL) return -1;
    //若原来处于choke状态且该peer连接未被关闭
    if(peer->state != CLOSING && peer->peer_choking == 1) {
        peer->peer_choking = 0;
        //若对该peer感兴趣,则构造request消息请求peer发送数据
        if(peer->am_interested == 1){ create_req_slice_msg(peer);}
        else {
            peer->am_interested = is_interested(&(peer->bitmap), bitmap);
            if(peer->am_interested == 1) create_req_slice_msg(peer);
            else printf("Received unchoke but Not interested to IP:%s", peer->ip);
        }
        //更新一些成员的值
        peer->last_down_timestamp = 0;
        peer->down_count = 0;
        peer->down_rate = 0;
    }

    peer->start_timestamp  = time(NULL);
    return 0;
}

int process_interested_msg(Peer *peer, unsigned char *buff, int len) {

    if(peer == NULL || buff == NULL) return -1;

    if(peer->state !=CLOSING && peer->state == DATA) {
        peer->peer_interested = is_interested(bitmap, &(peer->bitmap));
        if(peer->peer_interested == 0) return -1;
        if(peer->am_choking == 0) create_chock_interested_msg(1, peer);
    }

    peer->start_timestamp = time(NULL);
    return 0;
}

int process_uninterested_msg(Peer *peer, unsigned char *buff, int len){

    if(peer == NULL || buff == NULL) return -1;

    if(peer->state != CLOSING && peer->state == DATA ){
        peer->peer_interested = 0;
        cancel_requested_list(peer);
    }

    peer->start_timestamp = time(NULL);
    return 0;
}

int process_have_msg(Peer *peer, unsigned char *buff, int len) {

    int rand_num;
    unsigned char c[4];

    if(peer == NULL || buff == NULL) return -1;
    srand(time(NULL));
    rand_num = rand() % 3; //生成一个 0-2的随机数
    if(peer->state != CLOSING && peer->state == DATA) {
        c[0] = buff[5]; c[1] = buff[6];
        c[2] = buff[7]; c[3] = buff[8];
        //更新该peer的位置
        if(peer->bitmap.bitfield != NULL)
            set_bit_value(&(peer->bitmap), char_to_int(c), 1);
        if(peer->am_interested == 0) {
            peer->am_interested = is_interested(&(peer->bitmap),bitmap);
            //原来的对peer不感兴趣变为感兴趣,发送interested消息
            if(peer->am_interested == 1) create_chock_interested_msg(2, peer);
        } else {//收到3个hava则发一个iinterested消息
            if(rand_num == 0) create_chock_interested_msg(2, peer);
        }
    }

    peer->start_timestamp = time(NULL);
    return 0;
}

int process_cancel_msg(Peer *peer, unsigned char *buff, int len){

    unsigned char c[4];
	int           index, begin, length;

    if(peer==NULL || buff==NULL)  return -1;
    c[0] = buff[5];  c[1] = buff[6];
	c[2] = buff[7];  c[3] = buff[8];
	index = char_to_int(c);
	c[0] = buff[9];  c[1] = buff[10];
	c[2] = buff[11]; c[3] = buff[12];
	begin = char_to_int(c);
	c[0] = buff[13]; c[1] = buff[14];
	c[2] = buff[15]; c[3] = buff[16];
	length = char_to_int(c);
    //在被请求主队列中删除指定的请求
	Request_piece *p, *q;
	p = q = peer->Requested_piece_head;
	while(p != NULL) {
		if( p->index==index && p->begin==begin && p->length==length ) {
			if(p == peer->Requested_piece_head)
				peer->Requested_piece_head = p->next;
			else
				q->next = p->next;
			free(p);
			break;
		}
		q = p;
		p = p->next;
	}

	peer->start_timestamp = time(NULL);
	return 0;
}

int process_bitfield_msg(Peer *peer, unsigned char *buff, int len) {

    unsigned char c[4];
    if(peer == NULL && buff == NULL) return -1;

    if(peer->state == HANDSHAKE || peer->state ==  SENDBITFIELD) {
        c[0] = buff[0]; c[1] = buff[1];
        c[2] = buff[2]; c[3] = buff[3];
        //若原先已经收到一个位置消息,则清空原来的位图
        if(peer->bitmap.bitfield != NULL) {
            free(peer->bitmap.bitfield);
            peer->bitmap.bitfield = NULL;
        }

        peer->bitmap.valid_length = bitmap->valid_length;
        if(bitmap->bitfield_length != char_to_int(c) -1) { //若收到一个错误的位图
            peer->state = CLOSING;
            //丢弃发送缓冲区中的数据
            discard_send_buffer(peer);
            clear_btcache_before_peer_close(peer);
            close(peer->socket);
            return -1;
        }
        //生成该pper的位图
        peer->bitmap.bitfield_length = char_to_int(c) -1;
        peer->bitmap.bitfield = (unsigned char*)malloc(peer->bitmap.bitfield_length);
        memcpy(peer->bitmap.bitfield, &buff[5], peer->bitmap.bitfield_length);

        //如果原状态为已握,收到位图后应该向peer发位图
        if(peer->state == HANDSHAKED) {
            create_bitfield_msg(bitmap->bitfield, bitmap->bitfield_length, peer);
            peer->state = DATA;
        }
        //如果原状态为已经发送位图,收到位图后可以进行data状态装备交换数据
        if(peer->state ==   SENDBITFIELD) {
            peer->state = DATA;
        }
        //根据位图判断peer是否对本客户端感兴趣
        peer->peer_interested = is_interested(bitmap, &(peer->bitmap));
        //判断对peer是否感兴趣,叵是则发送interested 消息
        peer->am_interested = is_interested(&(peer->bitmap), bitmap);

        if(peer->am_interested == 1) create_chock_interested_msg(2, peer);
    }

    peer->start_timestamp = time(NULL);
    return 0;

}

int process_request_msg(Peer *peer, unsigned char *buff, int len) {

    unsigned char c[4];
    int index, begin, length;
    Request_piece *request_piece, *p;

    if(peer == NULL || buff == NULL) return -1;

    if(peer->am_choking == 0 && peer->peer_interested == 1) {
        c[0] = buff[5]; c[1] = buff[6];
        c[2] = buff[7]; c[3] = buff[8];
        index = char_to_int(c);

        c[0] = buff[9]; c[1] = buff[10];
        c[2] = buff[11];c[3] = buff[12];
        begin = char_to_int(c);

        c[0] = buff[13]; c[1] = buff[14];
        c[2] = buff[15]; c[3] = buff[16];
        length = char_to_int(c);
        //查看该请求是否已存在,若已经存在,则不进行处理
        p = peer->Requested_piece_head;
        while(p != NULL) {

            if(p->index == index && p->begin == begin && p->length == length){
                break;
            }
            p=p->next;
        }
        if(p != NULL) return 0;

        //若请求加入到请求队列中
        request_piece = (Request_piece*)malloc(sizeof(Request_piece));
        if(request_piece == NULL) {
            printf("%s:%d error", __FILE__, __LINE__);
            return 0;
        }

        request_piece->index    = index;
        request_piece->begin    = begin;
        request_piece->length   = length;
        request_piece->next     = NULL;

        if(peer->Requested_piece_head == NULL) {
            peer->Requested_piece_head = request_piece;
        } else {
            p = peer->Requested_piece_head;
            while(p != NULL) p = p->next;
            p->next = request_piece;
        }
        //打印提示信息
        printf("***add q request FROM IP:%s index:%-6d begin:%-6x***\n", peer->ip, index, begin);

    }

    peer->start_timestamp = time(NULL);
    return 0;
}

int process_piece_msg(Peer *peer, unsigned char *buff, int len) {

    unsigned char c[4];
    int index, begin,length;
    Request_piece *p;

    if(peer == NULL || buff == NULL) return -1;
    if(peer->peer_choking == 0) {
        c[0] = buff[0]; c[1] = buff[1];
        c[2] = buff[2]; c[3] = buff[3];
        length = char_to_int(c) - 9;

        c[0] = buff[5]; c[1] = buff[6];
        c[2] = buff[7]; c[3] = buff[8];
        index = char_to_int(c);

        c[0] = buff[9]; c[1] = buff[10];
        c[2] = buff[11];c[3] = buff[12];
        begin = char_to_int(c);
        //判断收到slice是否请求的
        p = peer->Requested_piece_head;
        while(p != NULL) {
            if(p->index == index && p->begin == begin && p->length)
                break;
            p = p->next;
        }
        if(p == NULL) { printf("did not found matched request\n"); return -1; }
        //开始记时,并累计收到数据的字节数
        if(peer->last_down_timestamp == 0)
            peer->last_down_timestamp = time(NULL);
        peer->down_count += length;
        peer->down_total +=  length;
        //将收到的数据写入缓冲区
        write_slice_to_btcache(index, begin, length, buff+13, length, peer);
        //生成请求数据的消息,要求继续发送数据
        create_req_slice_msg(peer);
    }

    peer->start_timestamp = time(NULL);
    return 0;
}

int parse_response(Peer *peer) {

    unsigned char btkeyword[20];
    unsigned char keep_alive[4] = {0x0, 0x0, 0x0, 0x0, };
    int index;
    unsigned char *buff = peer->in_buff;    //in_buff为接收缓冲区
    int len = peer->buff_len;               //buff_llen为接收缓冲中有效数据的长度

    if(buff == NULL || peer == NULL) return -1;

    btkeyword[0] = 19;
    memcpy(&btkeyword[1], "BitTorrent Protocol",19);    //BitTorrent协议关键字
    //分别处理12种消息
    for(index = 0; index < len;) {

        if ( (len-index >= 68) && (memcpy(&buff[index], btkeyword, 20) == 0 ) ) {
            process_handshake_msg(peer, buff+index, 68);
            index += 68;
        } else if( ((len-index) >= 4) && (memcpy(&buff[index], keep_alive, 4) == 0) ) {
            process_keep_alive_msg(peer, buff+index, 4);
            index += 4;
        } else if ( ((len-index) >= 5) && (buff[index+4]) == CHOKE ) {
            process_choke_msg(peer, buff+index, 5);
            index +=5;
        } else if( ((len-index) >= 5) && (buff[index+4]) == UNCHOKE ) {
            process_unchoke_msg(peer, buff+index, 5);
            index += 5;
        }else if ( ((len-index) >= 5) && (buff[index+4]) == INTERESTED  ) {
            process_interested_msg(peer, buff+index, 5);
            index += 5;
        }else if ( ((len-index) >= 5) && (buff[index+4]) == UNINTERESTED  ) {
            process_uninterested_msg(peer, buff+index, 5);
            index += 5;
        } else if( ((len-index) >= 9) && (buff[index+4]) == HAVE ) {
            process_have_msg(peer, buff+index, 9);
            index += 9;
        } else if( ((len-index) >= 5) && (buff[index+4]) == BITFIELD ) {
            process_bitfield_msg(peer, buff+index, peer->bitmap.bitfield_length +5);
            index += peer->bitmap.bitfield_length + 5;
        } else if ( ((len-index) >=  17) && (buff[index+4]) == REQUEST ) {
            process_request_msg(peer, buff+index, 17);
            index += 17;
        } else if( ((len-index) >=  13) && (buff[index+4]) == PIECE ) {
            unsigned char c[4];
            int length;

            c[0] = buff[index];   c[1] = buff[index+1];
            c[2] = buff[index+2]; c[3] = buff[index+3];
            length = char_to_int(c) - 9;

            process_piece_msg(peer, buff+index, length+13);
            index += length + 13;   //length+13为piece消息的长度
        } else if ( ((len-index) >=  17) && (buff[index+4]) == CANCEL) {
            process_cancel_msg(peer, buff+index, 17);
            index += 17;
        } else if ( ((len-index) >=  7) && (buff[index+4]) == PORT ) {
            index += 7;
        } else {
            //如果是未知的消息类型,则跳过不予处理
            unsigned char c[4];
            int length;
            if(index + 4 < len) {
                c[0] = buff[index];   c[1] = buff[index+1];
                c[2] = buff[index+2]; c[3] = buff[index+3];
                length = char_to_int(c);

                if(index+4+length <= len) { index += 4+length; continue; }
            }
            //如果是一条错误的消息,清空接收缓冲区
            peer->buff_len = 0;
            return -1;
        }

    }

    peer->buff_len = 0;
    return 0;
}

int parse_response_uncomplete_msg(Peer *p, int ok_len) {

    char *tmp_buff;
    int tmp_buff_len;

    //分配存储空间,并保存接收缓冲区中不完整的消息
    tmp_buff_len = p->buff_len - ok_len;
    if(tmp_buff_len <= 0) return -1;
    tmp_buff = (char*)malloc(tmp_buff_len);
    if(tmp_buff == NULL){
        printf("%s:%d error\n", __FILE__, __LINE__);
        return -1;
    }

    memcpy(tmp_buff, p->in_buff+ok_len, tmp_buff_len);
    //处理接收缓冲中前面 完整的消息
    p->buff_len = ok_len;
    parse_response(p);
    //将不完事的消息拷贝到接收缓冲的开始处
    memcpy(p->in_buff, tmp_buff, tmp_buff_len);
    p->buff_len = tmp_buff_len;
    if(tmp_buff != NULL) free(tmp_buff);

    return 0;
}

int prepare_send_have_msg() {

    Peer *p = peer_head;
    //int i;
    if(peer_head == NULL || have_piece_index[0] == -1) return -1;
    //if(hava_piece_index[0] == -1) return -1;

    while(p != NULL) {
        for(int i = 0; i < 64; i++) {
            if(have_piece_index[i] != -1) create_have_msg(have_piece_index[i], p);
            else break;
        }
        p =  p->next;
    }

    for(int i = 0; i < 64; i++) {
        if(have_piece_index[i] == -1) break;
        else have_piece_index[i] = -1;
    }

    return 0;
}

int create_response_message(Peer *peer) {

    if(peer == NULL) return -1;
    if(peer->state == INITIAL) { //处于initial状态时主动发握手消息
        create_handshake_msg(info_hash, peer_id, peer);
        peer->state = HALFSHAKED;
        return 0;
    }

    if(peer->state == HANDSHAKED) { //处于已经握手状态,主动发送图消息
        if(bitmap == NULL) return -1;
        create_bitfield_msg(bitmap->bitfield,bitmap->bitfield_length,peer);
        peer->state = SENDBITFIELD;
        return 0;
    }
    //如果条件允许(未将该pper阻塞,且peer发送过请求).则主动发送piece消息
    if(peer->am_choking == 0 && peer->Requested_piece_head != NULL) {
        Request_piece *req_p = peer->Requested_piece_head;
        int ret = read_slice_for_send(req_p->index, req_p->begin, req_p->length, peer);
        if(ret < 0) { printf("read_slice_for_send ERROR\n");}
        else {
            if(peer->last_up_timestamp == 0)
                peer->last_up_timestamp = time(NULL);
            peer->up_count += req_p->length;
            peer->up_total += req_p->length;

            peer->Requested_piece_head = req_p->next;

            printf("*** sending a slice TO:%s index:%-5d begin: %-5x ***\n", peer->ip, req_p->index, req_p->begin);
            free(req_p);
            return 0;
        }
    }
    //如果3分钟没有收到任何消息关闭连接
    time_t now = time(NULL);
    long interval1 = now - peer->start_timestamp;
    if(interval1 > 180){
        peer->state = CLOSING;
        //丢弃发送缓冲区中的数据
        discard_send_buffer(peer);
        //将从该pper处下载到的不足一个piece的数据删除
        clear_btcache_before_peer_close(peer);
        close(peer->socket);
    }
    //如果45秒没有发送和接收到消息,则发送一个keep_alive消息
    long interval2 = now - peer->recet_timestamp;
    if(interval1 > 45 && interval2 > 45 && peer->msg_len == 0)
        create_keep_alive_msg(peer);

    return 0;

}


void discard_send_buffer(Peer *peer) {

    struct linger lin;
    int lin_len;

    lin.l_onoff = 1;
    lin.l_linger = 0;
    lin_len = sizeof(lin);
    //通过设置套接字选项来丢弃未发送的数据
    if(peer->socket >0) {
        setsockopt(peer->socket, SOL_SOCKET, SO_LINGER, (char *)&lin, lin_len);
    }
}
