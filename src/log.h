#ifndef LOG_H
#define LOG_H

//�û���¼�������Ϊ
void logcmd(char *fmt, ...);

//����־�ļ�
int init_logfile(char *filename);
//������ ������־��¼���ļ�
int logfile(char *file, int line, char *msg);

#endif // LOG_H
