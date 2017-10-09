#incldue <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <netinet/in.h>
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

int *sock = NULL;
struct sosckaddr_in *tracker= NULL;
int *valid = NULL;
int tracker_count = 0;

int *peer_sock = NULL;
struct sockaddr_in *peer_addr = NULL;
int *peer_valid = NULL;
int peer_count = 0;

int response_len = 0;
int response_index = 0;
char *tracker_response= NULL;




int download_upload_with_peers() {
    Peer *p;
    int ret, max_sockefd, i;

    int connect_tracker, connecting_tracker;
    int connect_peer, connecting_peer;
    time_t last_time[3],now_time;

    time_t start_connect_tracker;
    time_t start_connect_peer;
    fd_set rset, wset;
    struct timeval tmval;

    now_time = time(NULL);
    last_time[0] = now_time;
    last_time[1] = now_time;
    last_time[2] = now_time;
    connect_tracker = 1;
    connecting_tracker = 0;
    connect_peer = 0;
    connecting_peer  = 0;

    for(;;) {
        max_sockefd = 0;
        now_time = time(NULL);

        if(now_time - last_time[0] >= 10) {
            if(download_pices_num > 0 && peer_head != NULL) {
                compute_rate();
                select_unchkoke_peer();
                last_time[0] = now_time;
            }
        }

        if(now_time-last_time[i] >= 30) {
            if(download_pices_num > 0 && peer_head != NULL) {
                select_optunchkoke_peer();
                last_time[1] = now_time;
            }
        }

        if( (now_time - last_time[2] >= 300 || connect_tracker == 1)&&
           connecting_tracker != 1 && connect_peer != 1 && connecting_peer != 1) {

            ret = prepare_connect_peer(&max_sockefd);
            if(ret < 0) {printf("prepare_connect_tracker\n");return -1;}
            connect_tracker = 0;
            connecting_tracker = 1;
            start_connect_tracker = now_time;
        }

        if(connect_peer == 1) {
            ret = prepare_connect_peer(&max_sockefd);
            if(ret < 0) {printf("prepare_connect_tracker\n");return -1;}

            connect_peer = 0;
            connecting_peer = 1;
            start_connect_peer = now_time;
        }

        FD_ZERO(&rset);
        FD_ZERO(&wset);

        if(connecting_tracker == 1) {
            int flag = 1;
            if(now_time - start_connect_tracker > 10) {
                for(i=0;i<tracker_count;i++)
                    if(valid[i] != 0) close(sock[i]);
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
        if(connecting_peer == 1) {
            int flag = 1;
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
        if(ret < 0) {
            printf("%s:%d error\n", __FILE__, __LINE__);
            perror("select error");
            break;
        }
        if(ret == 0) continue;

        prepare_send_have_msg();
        p = peer_head;
        while(p != NULL) {
            if(p->state !=CLOSING && FD_ISSET(p->socket, &rset)) {
                ret = recv(p->socket, p->in_buff+p->buff_len, MSG_SIZE-p->buff_len,0);
                if(ret <=0) {
                    p->state = CLOSING;
                    discard_send_buffer(p);
                    clear_btcache_before_peer_close(p);
                    close(p->socket);
                }else {
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
                    create_response_message(p);
                    if(p->msg_len > 0) {
                        memcpy(p->out_msg_copy, p->out_msg, p->msg_len);
                        p->msg_copy_len = p->msg_len;
                        p->msg_len = 0;
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
            for(i=0; i< tracker_count; i++) {
                if(valid[i] == -1){
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
                    unsigned short listen_port = 33550;
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
                        }
                    }
                }
            }
        }

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
                    if(error === 0) {
                        peer_valid[i] =1;
                        add_peer_node_to_peerlist(&peer_sock[i], peer_addr[i]);
                    }
                }
            }
        }

        p = peer_head;
        while(p != NULL) {
            if(p->state == CLOSING) {
                del_peer_node(p);
                p = peer_head;
            }else {
                p=p->next;
            }
        }
        if(download_pices_num == pieces_length / 20) {
            printf("+++++ All Files Downloaded Sucessfully +++++\n");
            break;
        }
    }

    return 0;
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

void print_process_into() {
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

void release_memory_in_torrent() {
    if(sock    != NULL)  { free(sock);    sock = NULL; }
	if(tracker != NULL)  { free(tracker); tracker = NULL; }
	if(valid   != NULL)  { free(valid);   valid = NULL; }

	if(peer_sock  != NULL)  { free(peer_sock);  peer_sock  = NULL; }
	if(peer_addr  != NULL)  { free(peer_addr);  peer_addr  = NULL; }
	if(peer_valid != NULL)  { free(peer_valid); peer_valid = NULL; }
	free_peer_addr_head();
}

