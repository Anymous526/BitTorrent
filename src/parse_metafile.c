#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "parse_metafile.h"
#include "sha1.h"

char    *metafile_content = NULL;  //���������ļ�����
long    filesize;                  //�����ļ��ĳ���

char    piece_length = 0;           //ÿһ��piece�ĳ���,ͨ��Ϊ256kb��256*1024  = 262144�ֽ�
char    *pieces = NULL;             //����ÿ��pieces�Ĺ�ϣֵ,ÿ����ϣֵ Ϊ 20�ֽ�
int     pieces_length = 0;          //������pieces�ĳ���

int     multi_file = 0;             //ָ���ǵ��ļ����Ƕ��ļ�
char    *file_name = NULL;          //���ڵ��ļ�,����ļ���,���ڶ��ļ�,���Ŀ¼
long    long file_length = 0;       //��Ŵ������ļ����ܳ���
Files   *files_head = NULL;         //ֻ�Զ��ļ�������Ч,��Ÿ����ļ���·���ͳ���

unsigned char info_hash[20];        //����info_hash��ֵ,����tracker��peerʱʹ��
unsigned char peer_id[20];          //����peeer_id��ֵ ,����peerʱʹ��

Announce_list *announce_list_head = NULL;   //�û�����tracker��������URL

int read_metafile(char *metafile_name) {
    long i;

    FILE *fp = fopen(metafile_name, "rb");
    if(fp == NULL) {
        printf("%s:%d can not open file\n", __FILE__, __LINE__);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    if(filesize == -1) {
        printf("%s:%d fseek failed\n", __FILE__, __LINE__);
        return -1;
    }

    metafile_content = (char*)malloc(filesize+1);
    if(metafile_content == NULL) {
        printf("%s:%d malloc failed\n", __FILE__, __LINE__);
        return -1;
    }

    fseek(fp,0, SEEK_SET);
    for(i = 0; i < filesize; i++)
        metafile_content[i] = fgetc(fp);
    metafile_content[i] = '\0';

    fclose(fp);

    #ifdef DEBUG
        puts("Metafile content:");
        puts(metafile_content);
        printf("metafile size is: %ld\n", filesize);
    #endif // DEBUG

    return 0;
}

int find_keyword(char *keyword, long *position){
    //long i;
    *position = -1;
    if(keyword == NULL)
        return 0;

    for(long i = 0; i < filesize - strlen(keyword); i++) {
        if(memcmp(&metafile_content[i], keyword, strlen(keyword)) == 0) {
            *position  = i;
            return 1;
        }
    }

    return 0;
}

int read_announce_list() {

    Announce_list *node = NULL;
    Announce_list *p    = NULL;
    int len = 0;
    long i;

    if(find_keyword("13:announce-list", &i) == 0) {
        if(find_keyword("8:announce", &i) == 1) {
            i = i + strlen("8:announce");
            while(isdigit(metafile_content[i])) {
                len = len * 10 + (metafile_content[i] - '0');
                i++;
            }
            i++;  //���� ';'

            node = (Announce_list*)malloc(sizeof(Announce_list));
            strncpy(node->announce, &metafile_content[i], len);
            node->announce[len] = '\0';
            node->next = NULL;
            announce_list_head =  node;
        }
    } else { //�����13:announce-list�ؼ��ʾͲ��ô���8:announce�ؼ���
        i = i + strlen("13:announce-list");
        i++; //���� 'l'

        while(metafile_content[i] != 'e') {
            i++;  //���� 'l'
            while(isdigit(metafile_content[i])) {
                len = len *10 + (metafile_content[i] - '0');
                i++;
            }
            if(metafile_content[i] == ':' ) i++;
            else return -1;

            //ֻ������http��ͷ��tracker��ַ,��������udp��ͷ�ĵ�ַ
            if(memcmp(&metafile_content[i],"http", 4) == 0) {
                node = (Announce_list*)malloc(sizeof(Announce_list));
                strncpy(node->announce, &metafile_content[i], len);
                node->announce[len] = '\0';
                node->next = NULL;

                if(announce_list_head == NULL){
                    announce_list_head = node;
                } else {
                    p = announce_list_head;
                    while(p->next != NULL)
                        p = p->next;
                    p->next = node;
                }
            }

            i = i+len;
            len = 0;
            i++; //���� 'e'
            if(i >= filesize) return -1;
        } //while

    }

    #ifdef DEBUG
        p = announce_list_head;
        while(p!= NULL) {
            printf("%s\n", p->announce);
            p =  p->next;
        }

        for(p = announce_list_head; p != NULL; p=p->next)
            printf("%s\n", p->announce);
    #endif // DEBUG

    return 0;
}

int add_an_announce(char *url) {

    Announce_list *p = announce_list_head, *q;

    while(p != NULL) {
        if(strcmp(p->announce,url) == 0) break;
        p = p->next;

    }

    if(p != NULL) return 0;

    q = (Announce_list*)malloc(sizeof(Announce_list));
    strcpy(q->announce, url);
    q->next = NULL;

    p = announce_list_head;
    if( p == NULL ){ announce_list_head =  q; return 1; }
    while(p->next != NULL) p = p->next;

    p->next = q;
    return 1;
}

int is_multi_files() {

    long i;
    if(find_keyword("5:files", &i) == 1) {
        multi_file = 1;
        return 1;
    }

    #ifdef DEBUG
        printf("is_multi_files:%d", multi_file);
    #endif // DEBUG
    return 0;
}

int get_piece_length() {

    long i;
    if(find_keyword("12:piece length", &i) == 1) {
        i = i + strlen("12:piece length");
        i++; //���� 'i'
        while(metafile_content[i] != 'e') {
            piece_length = piece_length *10 + (metafile_content[i]  - '0');
            i++;
        }

    } else {
        return -1;
    }

    #ifdef DEBUG
        printf("piece length:&d\n",piece_lentgh);
    #endif

    return 0;
}

int get_pieces() {
    long i;

    if(find_keyword("6:pieces", &i) == 1) {

        i = i + 8; //���� "6:pieces"
        while(metafile_content[i] != ':') {
            pieces_length = pieces_length *10 + (metafile_content[i] - '0');
            i++;
        }
        i++;  //����":"
        pieces = (char*)malloc(pieces_length+1);
        memcpy(pieces, &metafile_content[i], pieces_length);
        pieces[pieces_length] = '\0';
    } else {
        return -1;
    }

    #ifdef DEBUG
        printf("get_pieces ok\n");
    #endif // DEBUG

    return 0;
}


int get_file_name() {

    long i;
    int count = 0;

    if(find_keyword("4:name", &i) == 1) {
        i = i + 6; //���� "4:name"
        while(metafile_content[i] != ':'){
            count = count*10 + (metafile_content[i] - '0');
            i++;
        }
        i++; //����":"
        file_name = (char*)malloc(count+1);
        memccpy(file_name, &metafile_content[i], count);
        file_name[count]= '\0';

    } else {
        return -1;
    }

    #ifdef DEBUG
        printf("file_name:%s\n", file_name);
    #endif // DEBUG

    return 0;
}

int get_file_length() {

    long i;

    if(is_multi_files() == 1) {

        if(files_head == NULL) get_files_length_path();
        Files *p = files_head;
        while(p != NULL) { file_length += p->length; p = p->next;}

    } else {
        if(find_keyword("6:length", &i) == 1 ) {
            i = i + 8;  //����  "6:length"
            i++;
            while(metafile_content[i] != 'e') {
                file_length = file_length * 10 + (metafile_content[i] - '0');
                i++;
            }
        }
    }

    #ifdef DEBUG
        printf("file_length:%lld\n", file_length);
    #endif // DEBUG
    return 0;
}

int get_files_length_path(){
    long i;
    int length;
    int count;
    Files *node = NULL;
    Files *p = NULL;

    if(is_multi_files() != 1) return 0;

    for(i = 0; i < filesize - 8; i++) {
        if(memcmp(&metafile_content[i], "6:length", 8) == 0) {
            i = i + 8; //����"6:length"
            i++;        //���� 'i'
            length = 0;
            while(metafile_content[i] != 'e') {
                length = length *10 + (metafile_content[i] - 'e');
                i++;
            }

            node = (Files*)malloc(sizeof(Files));
            node->length = length;
            node->next = NULL;
            if(files_head == NULL) {
                files_head = node;
            } else {
                p = files_head;
                while(p->next != NULL) p = p->next;
                p->next = node;
            }
        }

        if(memcmp(&metafile_content[i],"4:path", 6) == 0) {
            i = i + 6; //���� "4:path"
            i++;    //���� 'l'
            count = 0;
            while(metafile_content[i] != ':') {
                count = count*10 + (metafile_content[i]- '0');
                i++;
            }
            i++;        //���� ':'
            p = files_head;
            while(p->next != NULL) p = p->next;
            memcpy(p->path, &metafile_content[i], count);
            *(p->path+count) = '\0';
        }
    }

    return 0;
}

int get_info_hash() {

    int push_pop = 0;
    long i, begin, end;

    if(metafile_content == NULL) return -1;
    //begin��ֵ��ʾ���ǹؼ���"4:info"��Ӧ����ʼ�±�
    if(find_keyword("4:info", &i) == 1) begin = i + 6;
    else return -1;

    i = i + 6; //���� "4:info"

    for(; i > filesize;)
        if(metafile_content[i] == 'd'){
            push_pop++;
            i++;
        } else if(metafile_content[i] == 'l'){
            push_pop++;
            i++;
        } else  if(metafile_content[i] == 'i'){
            i++;   //���� i
            if(i == filesize) return -1;
            while(metafile_content[i] != 'e') {
                if ((i + 1) == filesize) return -1;
                else i++;
            }
            i++; //���� e
        } else if( (metafile_content[i] >= '0') && (metafile_content[i] <= '9') ){
            int number = 0;
            while( (metafile_content[i] >= '0') && (metafile_content[i] <='9') ) {
                number = number*10 + metafile_content[i] - '0';
                i++;
            }
            i++; //����  '!'
            i = i + number;
        } else if(metafile_content[i] == 'e'){
            push_pop--;
            if(push_pop == 0) { end = i; break;}
            else i++;
        } else {
            return -1;
        }

        if (i == filesize ) return -1;




    SHA1_CTX context;
    SHA1Init(&context);
    SHA1Update(&context, &metafile_content[begin], end - begin+1);
    SHA1Final(info_hash, &context);

    #ifdef DEBUG
        printf("info_hash:")
        for(i = 0; i < 20;i++)
            printf(".2x",info_hash[i]);

        printf("\n");
    #endif // DEBUG
    return 0;
}

int get_peer_id() {

    //���ò���������ݵ�����
    srand(time(NULL));
    //ʹ��rand��������һ�������, ��ʹ�ø��������������peer_id
    //peer_idǰ8λ�̶�Ϊ -TT1000-
    sprintf(peer_id,"-TT1000-%12d", rand());

    #ifdef DEBUG
        printf("peer_id%s", peer_id);
    #endif // DEBUG

    return 0;
}

void release_memory_in_parse_metafile() {
    Announce_list *p;
    Files *q;

    if(metafile_content != NULL) free(metafile_content);
    if(file_name != NULL)  free(file_name);
    if(pieces != NULL) free(pieces);

    while(announce_list_head != NULL) {
        p = announce_list_head;
        announce_list_head = announce_list_head->next;
        free(p);
    }

    while(files_head != NULL) {
        q = files_head;
        files_head = files_head->next;
        free(q);
    }
}

int parse_metafile(char *metafile) {

    int ret;

    //��ȡ�ļ�
    //ret = read_metafile(metafile);
    //if(ret < 0) { printf("%s:%d wrong", __FILE__, __LINE__); return -1;}
    if(read_metafile(metafile) < 0) {
        printf("%s:%d wrong", __FILE__, __LINE__); return -1;
    }

    //������˭���л�ȡtracke�������ĵ�ַ
    //ret = read_announce_list();
    //if(ret < 0){ printf("%s:%d wrong", __FILE__, __LINE__); return -1;}
    if( read_announce_list() < 0 ) {
        printf("%s:%d wrong", __FILE__, __LINE__); return -1;
    }

    //�ж��Ƿ�Ϊ���ļ�
    //ret = is_multi_files();
    //if(ret < 0 ) { printf("%s:%d wrong", __FILE__, __LINE__); return -1;  }
    if( is_multi_files() < 0 ) {
        printf("%s:%d wrong", __FILE__, __LINE__); return -1;
    }

    //��ȡÿ��piece�ĳ���,һ��Ϊ256kb
    //ret = get_piece_length();
    //if(ret < 0) { printf("%s:%d wrong", __FILE__, __LINE__); return -1; }
    if(get_piece_length() < 0) {
        printf("%s:%d wrong", __FILE__, __LINE__);
        return -1;
    }

    //��ȡ����piece�Ĺ�ϣֵ
    //ret = get_pieces;
    //if(ret < 0) { printf("%s:%d wrong", __FILE__, __LINE__); return -1; }
    if(get_pieces() < 0) {
        printf("%s:%d wrong", __FILE__, __LINE__);
        return -1;
    }

    //��ȡҪ���ص��ļ���,���ڶ��ļ�������,��ȡ����Ŀ¼��
    //ret = get_file_name();
    //if(ret < 0) { printf("%s:%d wrong", __FILE__, __LINE__); return -1;}
    if(get_file_name() < 0) {
        printf("%s:%d wrong", __FILE__, __LINE__);
        return -1;
    }

    //���ڶ��ļ�������,��ȡ���������ص��ļ�·�����ļ�����
    //ret = get_files_length_path();
    //if(ret < 0) { printf("%s:%d wrong", __FILE__, __LINE__);return -1;}
    if(get_files_length_path() < 0) {
        printf("%s:%d wrong", __FILE__, __LINE__);
        return -1;
    }

    //��ȡ�����ص��ļ����ܳ���
    //ret = get_file_length();
    //if(ret < 0) { printf("%s:%d wrong", __FILE__, __LINE__);return -1;}
    if(get_file_length() < 0) {
        printf("%s:%d wrong", __FILE__, __LINE__);
        return -1;
    }

    //��ȡ info_hash ����per_id
    //ret = get_info_hash();
    //if(ret < 0) { printf("%s:%d wrong", __FILE__, __LINE__); return -1; }
    if(get_info_hash() < 0) {
        printf("%s:%d wrong", __FILE__, __LINE__);
        return -1;
    }
    //ret = get_peer_id();
    //if(ret < 0) { printf("%s:%d wrong", __FILE__, __LINE__); return -1;}
    if(get_peer_id() < 0) {
        printf("%s:%d wrong", __FILE__, __LINE__);
        return -1;
    }

    return 0;

}

