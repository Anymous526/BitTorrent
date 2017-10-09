#ifndef PASE_METAFILE
#define PASE_METAFILE

//保存从种子文件中获取的tracker的URL
typedef struct _Announce_list {
    char    announce[128];
    struct _Announce_list *next;
}Announce_list;

//保存各个待下载文件的路径和长度
typedef struct _Files{
    char    path[256];
    long    length;
    struct  _File *next;
}Files;

//读取种子文件
int read_metafile(char *metafile_name);
//在种子文件中查询某个关键词
int find_keyword(char *keyword, long *position);
//获取各个tracker服务器的地址
int read_announce_list();
//向tracker列表添加一个URL
int add_an_announce(char *url);

//获取各个piece的长雅, 一般为256kb
int get_piece_length();
//读取各个piece的哈希值
int get_pieces();

//判断下载的是单个文件还是多个文件
int is_multi_files();
//获取文件名,对于多文件, 获取的是目录名
int get_file_name();
//获取待下载文件的总长雅
int get_file_length();
//获取文件的路径和长度,对多文件种子有效
int get_files_length_path();

//由info关键词对应的值计算info_hash
int get_info_hash();
//生成peer_id,每个peer都有一个20字节的peer_id
int get_peer_id();

//释放parse_metafile.c中动态分配的内存
void release_memory_in_parse_metafile();

//庄胜本文件中定义的函数,完成解释种子文件
int parse_metafile(char *metafile);

#endif // PASE_METAFILE
