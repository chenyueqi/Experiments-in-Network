//
// 文件名: stcp_client.h
//
// 描述: 这个文件包含客户端状态定义, 一些重要的数据结构和客户端STCP套接字接口定义. 你需要实现所有这些接口.
//
// 创建日期: 2015年

#ifndef STCPCLIENT_H
#define STCPCLIENT_H

#include <pthread.h>
#include <assert.h>
#include "../common/seg.h"

//FSM中使用的客户端状态
#define	CLOSED 1
#define	SYNSENT 2
#define	CONNECTED 3
#define	FINWAIT 4

//在发送缓冲区链表中存储段的单元
typedef struct segBuf {
        seg_t seg;
        unsigned int sentTime;
        struct segBuf* next;
} segBuf_t;

//客户端传输控制块. 一个STCP连接的客户端使用这个数据结构记录连接信息.   
typedef struct client_tcb {
	unsigned int server_nodeID;        //服务器节点ID, 类似IP地址, 本实验未使用
	unsigned int server_portNum;       //服务器端口号
	unsigned int client_nodeID;     //客户端节点ID, 类似IP地址, 本实验未使用
	unsigned int client_portNum;    //客户端端口号
	unsigned int state;     	//客户端状态
	unsigned int next_seqNum;       //新段准备使用的下一个序号 
	pthread_mutex_t* bufMutex;      //发送缓冲区互斥量
	segBuf_t* sendBufHead;          //发送缓冲区头
	segBuf_t* sendBufunSent;        //发送缓冲区中的第一个未发送段
	segBuf_t* sendBufTail;          //发送缓冲区尾
	unsigned int unAck_segNum;      //已发送但未收到确认段的数量
} client_tcb_t;

client_tcb_t* gtcb_table[MAX_TRANSPORT_CONNECTIONS];
int gson_conn;

void stcp_client_init(int conn);

int stcp_client_sock(unsigned int client_port);

int stcp_client_connect(int socked, unsigned int server_port);

// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length);

// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd);

// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd);

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg);

// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

#endif
