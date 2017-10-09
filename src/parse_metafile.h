#ifndef PASE_METAFILE
#define PASE_METAFILE

//����������ļ��л�ȡ��tracker��URL
typedef struct _Announce_list {
    char    announce[128];
    struct _Announce_list *next;
}Announce_list;

//��������������ļ���·���ͳ���
typedef struct _Files{
    char    path[256];
    long    length;
    struct  _File *next;
}Files;

//��ȡ�����ļ�
int read_metafile(char *metafile_name);
//�������ļ��в�ѯĳ���ؼ���
int find_keyword(char *keyword, long *position);
//��ȡ����tracker�������ĵ�ַ
int read_announce_list();
//��tracker�б����һ��URL
int add_an_announce(char *url);

//��ȡ����piece�ĳ���, һ��Ϊ256kb
int get_piece_length();
//��ȡ����piece�Ĺ�ϣֵ
int get_pieces();

//�ж����ص��ǵ����ļ����Ƕ���ļ�
int is_multi_files();
//��ȡ�ļ���,���ڶ��ļ�, ��ȡ����Ŀ¼��
int get_file_name();
//��ȡ�������ļ����ܳ���
int get_file_length();
//��ȡ�ļ���·���ͳ���,�Զ��ļ�������Ч
int get_files_length_path();

//��info�ؼ��ʶ�Ӧ��ֵ����info_hash
int get_info_hash();
//����peer_id,ÿ��peer����һ��20�ֽڵ�peer_id
int get_peer_id();

//�ͷ�parse_metafile.c�ж�̬������ڴ�
void release_memory_in_parse_metafile();

//ׯʤ���ļ��ж���ĺ���,��ɽ��������ļ�
int parse_metafile(char *metafile);

#endif // PASE_METAFILE
