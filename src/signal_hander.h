#ifndef SIGNAL_HANDER
#define SIGNAL_HANDER

//做一些清理工作
void do_clear_work();

//处理一些信号
void process_signal(int sinno);
//设置信号处理函数
int set_signal_hander();

#endif // LOG_H
