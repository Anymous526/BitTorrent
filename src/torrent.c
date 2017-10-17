#incldue <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include "torrent.h"
#include "message.h"
#include "tracker.h"
#include "peer.h"
#include "policy.h"
#include "data.h"
#include "bitfield.h"
#include "parse_metafile.h"

//接收缓冲区的数据大家到threshold时,需要立即进行处理,否则缓冲区可能溢出
//18*1204bit取18kb是接收缓冲区的大小, 1500btye是以太网等局域网一个数据包的最大长度
#define threshold (18*1024 - 1500)

extern Announce_list *announce_list_head;
extern char *file_name;
extern long long file_length;
extern int piece_length;
extern char *pieces;
extern int pieces_length;
extern Peer *peer_head;

extern long long total_down, total_up;
extern float total_down_rate, total_up_rate;
extern int total_peers;
extern int download_pices_num;
extern Peer_addr *peer_addr_head;

int *sock = NULL;           //连接tracker的套接字
struct sosckaddr_in *tracker= NULL; //连接tracker时使用
int *valid = NULL;              //指示连接tracker的状态
int tracker_count = 0;          //为trackerer服务器的个数

int *peer_sock = NULL;             //连接peer的套接字
struct sockaddr_in *peer_addr = NULL;   //连接peer时使用
int *peer_valid = NULL; //指示连接peer在状态
int peer_count = 0; //尝试与多少个peer建立 连接

int response_len = 0;   //存放tracker回应的总长度
int response_index = 0; //存放tracker回应当前长度
char *tracker_response = NULL;   //存放tracker回应

int download_upload_with_peers() {
    Peer *p;
    int ret, max_sockefd, i;

    int connect_tracker, connecting_tracker;
    int connect_peer, connecting_peer;
    time_t last_time[3],now_time;

    time_t start_connect_tracker;   //开始连接tracker的时间
    time_t start_connect_peer;      //开始连接peer的时间
    fd_set rset, wset;              //select要监视的描述符集合
    struct timeval tmval;           //select函数的超时时间

    now_time = time(NULL);
    last_time[0] = now_time;        //上一次选择非阻塞的peer的时间
    last_time[1] = now_time;        //上一次选择优化非阻塞的时间
    last_time[2] = now_time;        //上一次连接Tracker服务器的时间
    connect_tracker = 1;            //是否需要连接tracker
    connecting_tracker = 0;         //是否正在连接peer
    connect_peer = 0;               //是否需要连接peer
    connecting_peer  = 0;           //是否正在连接peer

    for(;;) {
        max_sockefd = 0;
        now_time = time(NULL);

        //每隔10秒重新选择非阻塞的peer
        if(now_time - last_time[0] >= 10) {
            if(download_pices_num > 0 && peer_head != NULL) {
                compute_rate();
                select_unchkoke_peer();
                last_time[0] = now_time;
            }
        }

        //每隔30秒重新选择优化非阻塞peer
        if(now_time-last_time[i] >= 30) {
            if(download_pices_num > 0 && peer_head != NULL) {
                select_optunchkoke_peer();
                last_time[1] = now_time;
            }
        }

        //每隔5分钟连接一次Tracker,如果当前peer数为0也连接Tracker
        if( (now_time - last_time[2] >= 300 || connect_tracker == 1)&&
           connecting_tracker != 1 && connect_peer != 1 && connecting_peer != 1) {

            //由Tracker的URL获取Tracker的IP地址和端口
            ret = prepare_connect_peer(&max_sockefd);
            if(ret < 0) {printf("prepare_connect_tracker\n");return -1;}
            connect_tracker = 0;
            connecting_tracker = 1;
            start_connect_tracker = now_time;
        }

        //如果要连接新的peer,做准备工作
        if(connect_peer == 1) {
            //创建套接字,向peer发出连接请求
            ret = prepare_connect_peer(&max_sockefd);
            if(ret < 0) {printf("prepare_connect_tracker\n");return -1;}

            connect_peer = 0;
            connecting_peer = 1;
            start_connect_peer = now_time;
        }

        FD_ZERO(&rset);
        FD_ZERO(&wset);

        //将连接tracker的socket加入到待监控的集合中
        if(connecting_tracker == 1) {
            int flag = 1;
            //如果连接tracker超过10秒,则终止连接Tracker
            if(now_time - start_connect_tracker > 10) {
                for(i = 0;i < tracker_count; i++)
                    if(valid[i] != 0) close(sock[i]); //valid[i]的值为 -1, 1 ,2 时要监视
            } else {
              for(i=0;i<tracker_count;i++) {
                if(valid[i] != 0 && sock[i] > max_sockefd)
                    max_sockefd = sock[i];
                if(valid[i] == -1) {
                    FD_SET(sock[i],&rset);
                    FD_SET(sock[i],&wset);
                    if(flag == 1) flag = 0;
                } else if(valid[i] == 1) {
                    FD_SET(sock[i], &wset);
                    if(flag ==1) flag = 0;
                } else if(valid[i] == 2) {
                    FD_SET(sock[i], &rset);
                    if(flag == 1) flag = 0;
                }
              }
            }
        //说明连接Tracker结束 , 开始也peer建立连接
        if(flag == 1) {
            connecting_tracker = 0;
            last_time[2]= now_time;
            clear_connect_tracker();
            clear_tracker_response();
            if(peer_addr_head != NULL) {
                connect_tracker = 0;
                connect_peer = 1;
            } else {
                connect_tracker = 1;
            }
            continue;
        }
        }
        //将正在连接的peer的socket加入到待监视的集合中
        if(connecting_peer == 1) {
            int flag = 1;
            //如果连接peer超过10秒,则终止连接peer
            if(now_time - start_connect_peer > 10) {
                for(i=0;i<peer_count; i++) {
                    if(peer_valid[i] != 1) close(peer_sock[i]);
                }
            }else {
                for(i = 0; i< peer_count; i++) {
                    if(peer_valid[i] == -1) {
                        if(peer_sock[i] > max_sockefd)
                            max_sockefd = peer_sock[i];
                        FD_SET(peer_sock[i], &rset);
                        FD_SET(peer_sock[i], &wset);
                        if(flag == 1) flag = 0;
                    }
                }
            }

            if(flag == 1) {
                connecting_peer = 0;
                clear_connect_peer();
                if(peer_head == NULL) connect_tracker =1;
                continue;
            }
        }
        //将peer的socket成员加入到待监视的集合中
        connect_tracker = 1;
        p = peer_head;
        while(p!= NULL) {
            if(p->state != CLOSING &&p->socket > 0) {
                FD_SET(p->socket, &rset);
                FD_SET(p->socket, &wset);
                if(p->socket > max_sockfd) max_sockefd = p->socket;
                connect_tracker = 0;
            }
            p = p->next;
        }

        if(peer_head == NULL && (connecting_tracker == 1 || connecting_peer ==1))
            connect_tracker =0;
        if(connect_tracker == 1) continue;

        tmval.tv_sec = 2;
        tmval.tv_usec = 0;
        ret = select(max_sockefd+1, &rset, &wset, NULL, &tmval);
        if(ret < 0) { //select出错
            printf("%s:%d error\n", __FILE__, __LINE__);
            perror("select error");
            break;
        }
        if(ret == 0) continue;

        //添加hava消息,hava消息要发送给每一个peer,放在此处是为了方便处理
        prepare_send_have_msg();
        p = peer_head;
        while(p != NULL) {
            if(p->state != CLOSING && FD_ISSET(p->socket, &rset)) {
                ret = recv(p->socket, p->in_buff+p->buff_len, MSG_SIZE-p->buff_len,0);
                if(ret <=0) { //recv返回0说明对方关闭连接,返回负数说明出错
                    p->state = CLOSING;
                    //通过设置套接字选项来丢弃发送缓冲区中的数据
                    discard_send_buffer(p);
                    clear_btcache_before_peer_close(p);
                    close(p->socket);
                } else {
                    int completed, ok_len;
                    p->buff_len += len;
                    complete = is_complete_messgae(p->in_buff, p->buff_len, &ok_len);
                    if(completed == 1) parse_response(p);
                    else if(p->buff_len >= threshold)
                        parse_response_uncomplete_msg(p, ok_len);
                    else
                        p->start_timestamp = time(NULL);
                }
            }

            if(p->state != CLOSING &&FD_ISSET(p->socket, &wset)) {
                if(p->msg_copy_len == 0) {
                    //创建待发送的消息,并把生成的消息拷贝到发送缓冲区并发送
                    create_response_message(p);
                    if(p->msg_len > 0) {
                        memcpy(p->out_msg_copy, p->out_msg, p->msg_len);
                        p->msg_copy_len = p->msg_len;
                        p->msg_len = 0; //清空p->out_msg所存的消息
                    }
                }

                if(p->msg_copy_len > 1024) {
                    send(p->socket, p->out_msg_copy+p->msg_copy_index,1024,0);
                    p->msg_copy_len = p->msg_copy_len - 1024;
                    p->msg_copy_index = p->msg_copy_index +1024;
                    p->recet_timestamp = time(NULL);
                } else if(p->msg_copy_len <= 1024 && p->msg_copy_len > 0) {
                    send(p->socket, p->out_msg_copy+p->msg_copy_index,
                        p->msg_copy_len, 0);
                    p->msg_copy_len = 0;
                    p->msg_copy_index = 0;
                    p->recet_timestamp = time(NULL);
                }
            }
            p = p->next;
        }

        if(connecting_tracker == 1) {
            for(i = 0; i < tracker_count; i++) {
                if(valid[i] == -1){
                    //如果套接字可写且未发生错误,说明连接建立成功
                    if(FD_ISSET(sock[i], &wet)) {
                        int error, len;
                        error = 0;
                        len = sizeof(error);
                        ret = getsockopt(sock[i], SOL_SOCKET, SO_ERROR, &error,&len);
                        if(ret < 0) {
                            valid[i] = 0;
                            close(sock[i]);
                        }
                        if(error) {valid[i]=0; close(sock[i]);}
                        else {valid[i] = 1;}
                    }
                }
                if(valid[i0] == 1 && FD_ISSET(sock[i], &wet) ) {
                    char request[1024];
                    unsigned short listen_port = 33550; //本程序并未实例同监听某端口
                    unsigned long down = total_down;
                    unsigned long up = total_up;
                    unsigned long left;
                    left = (pieces_length/20-download_pices_num)*piece_length;

                    int num = i;
                    Announce_list *anouce = announce_list_head;
                    while(num > 0) {
                        anouce = anouce->next;
                        num--;
                    }
                    create_request(request, 1024, anouce, listen_port, down, up, left, 200);
                    write(sock[i], request, strlen(request));
                    valid[i] = 2;
                }

                if(valid[i] == 2 &&FD_ISSET(sock[i], &rset)) {
                    char buffer[2018];
                    char redirection[128];
                    ret = read(sock[i], buffer, sizeof(buffer));
                    if(ret > 0) {
                        if(response_len != 0) {
                            memccpy(tracker_response+response_index, buffer, ret);
                            response_index += ret;
                            if(response_index == response_len) {
                                parse_tracker_response2(track_response, response_len);
                                clear_track_response();
                                valid[i] = 0;
                                close(sock[i]);
                                last_time[2] = time(NULL);
                            }
                        } else if(get_response_type(buff, ret, &response_len) == 1) {
                            tracker_response = (char*)malloc(response_len);
                            if(tracker_response == NULL) printf("malloc error\n");
                            memcpy(tracker_response, buffer, ret);
                            response_index = ret;
                        } else {
                            ret = parse_tracker_response1(buffer, ret, redirection, 128);
                            if(ret == 1) add_an_announce(redirection);
                            valid[i] = 0;
                            close(sock[i]);
                            last_time[2] = time(NULL);
                        }// if(response_len != 0)
                    }// end if(ret > 0)
                }// end if(valid[i] == 2 && FD_ISSET(sock[i],&rset))
            }//end for(i = 0; i < tracker_count; i++)
        }// end if(valid[i] == -1)

        if(connecting_peer == 1) {
            for(i = 0; i < peer_count; i++) {
                if(peer_valid[i] == -1 &&FD_ISSET(peer_sock[i], &wset)) {
                    int error, len;
                    error = 0;
                    len = sizeof(error);
                    ret = getsockopt(peer_sock[i], SOL_SOCKET, SO_ERROR, &error, &len);
                    if(ret < 0) {
                        peer_valid[i] = 0;
                    }
                    if(error == 0) {
                        peer_valid[i] =1;
                        add_peer_node_to_peerlist(&peer_sock[i], peer_addr[i]);
                    }
                }//if 结束变速器
            } //for结束
        }//if结束

        //对处于cCLOSING状态的peer,将其从peer队列中删除
        //此处应该当非常小心,处理不当容易使用程序崩溃
        p = peer_head;
        while(p != NULL) {
            if(p->state == CLOSING) {
                del_peer_node(p);
                p = peer_head;
            }else {
                p=p->next;
            }
        }
        //判断是否已经下载完毕
        if(download_pices_num == pieces_length / 20) {
            printf("+++++ All Files Downloaded Sucessfully +++++\n");
            break;
        }
    } //end for(;;;)

    return 0;
}

void print_process_info() {
    char  info[256];
	float down_rate, up_rate, percent;

	down_rate = total_down_rate;
	up_rate   = total_up_rate;
	percent   = (float)download_piece_num / (pieces_length/20) * 100;
	if(down_rate >= 1024)  down_rate /= 1024;
	if(up_rate >= 1024)    up_rate   /= 1024;

	if(total_down_rate >= 1024 && total_up_rate >= 1024)
		sprintf(info,"Complete:%.2f%% Peers:%d Down:%.2fKB/s Up:%.2fKB/s \n",
				percent,total_peers,down_rate,up_rate);
	else if(total_down_rate >= 1024 && total_up_rate < 1024)
		sprintf(info,"Complete:%.2f%% Peers:%d Down:%.2fKB/s Up:%.2fB/s \n",
				percent,total_peers,down_rate,up_rate);
	else if(total_down_rate < 1024 && total_up_rate >= 1024)
		sprintf(info,"Complete:%.2f%% Peers:%d Down:%.2fB/s Up:%.2fKB/s \n",
				percent,total_peers,down_rate,up_rate);
	else if(total_down_rate < 1024 && total_up_rate < 1024)
		sprintf(info,"Complete:%.2f%% Peers:%d Down:%.2fB/s Up:%.2fB/s \n",
				percent,total_peers,down_rate,up_rate);

	//if(total_down_rate<1 && total_up_rate<1)  return;
	printf("%s",info);
}

int print_peer_list() {
    Peer *p = peer_head;
	int  count = 0;

	while(p != NULL) {
		count++;
		printf("IP:%-16s Port:%-6d Socket:%-4d\n",p->ip,p->port,p->socket);
		p = p->next;
	}

	return count;
}

void release_memory_in_torrent() {
    if(sock    != NULL)  { free(sock);    sock = NULL; }
	if(tracker != NULL)  { free(tracker); tracker = NULL; }
	if(valid   != NULL)  { free(valid);   valid = NULL; }

	if(peer_sock  != NULL)  { free(peer_sock);  peer_sock  = NULL; }
	if(peer_addr  != NULL)  { free(peer_addr);  peer_addr  = NULL; }
	if(peer_valid != NULL)  { free(peer_valid); peer_valid = NULL; }
	free_peer_addr_head();
}

void clear_connect_tracker() {
    if(sock != NULL) {free(sock); sock = NULL;}
    if(tracker != NULL) {free(tracker); tracker = NULL;}
    if(valid != NULL) {free(valid)l; valid = NULL};
    tracker_count = 0;
}

void clear_connect_peer() {
    if(peer_sock != NULL) {free(peer_sock); peer_sock = NULL;}
    if(peer_addr != NULL) {free(peer_addr); peer_addr = NULL;}
    if(peer_valid != NULL) {free(peer_valid); peer_valid = NULL;}
    peer_count  =0;

}

void clear_tracker_response() {
    if(tracker_response != NULL){
        free(tracker_response);
        tracker_response = NULL;
    }
    response_len = 0;
    response_index = 0;
}

