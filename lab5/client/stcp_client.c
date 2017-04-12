#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "stcp_client.h"

/*面向应用层的接口*/

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_client_init(int conn) {
  int i = 0;
  for(;i<MAX_TRANSPORT_CONNECTIONS;i++)
	gtcb_table[i] = NULL;
  gson_conn = conn;
  pthread_t thread;
  int rc = pthread_create(&thread, NULL, seghandler, NULL);
  if(rc) {
	fprintf(stderr, "ERROR; return code from pthread_create() of seghander is %d\n", rc);
	exit(-1);
  }
  return;
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
  int i = 0;
  for (;i<MAX_TRANSPORT_CONNECTIONS && gtcb_table[i]!=NULL;i++) {}
  if (i==MAX_TRANSPORT_CONNECTIONS)
	return -1;

  gtcb_table[i] = (client_tcb_t*)malloc(sizeof(client_tcb_t));
  gtcb_table[i]->client_portNum = client_port;
  gtcb_table[i]->state = CLOSED;
  gtcb_table[i]->next_seqNum = 0;
  // TODO more initialization
  return i;
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, unsigned int server_port) {
  client_tcb_t* tp;
  tp = gtcb_table[sockfd];
  if(tp == NULL) {
	fprintf(stderr, "fail! socket does not exit\n");
	return -1;
  }
  switch (tp->state) {
	case CLOSED: 
	  tp->server_portNum = server_port;
	  seg_t syn_seg;
	  syn_seg.header.src_port = tp->client_portNum;
	  syn_seg.header.dest_port = tp->server_portNum;
	  syn_seg.header.seq_num = tp->next_seqNum;
	  syn_seg.header.ack_num = 0;
	  syn_seg.header.length = 0;
	  syn_seg.header.type = SYN;
	  unsigned char cnt = 0;
	  tp->state = SYNSENT;
	  for (;cnt < SYN_MAX_RETRY; cnt++) {
		sip_sendseg(gson_conn, &syn_seg);
		printf("send SYN to server %u\n", cnt);
//		usleep(SYN_TIMEOUT/1000);
		sleep(1);
		if(tp->state == CONNECTED)
		  return 1;
	  }
	  tp->state = CLOSED;
	  return -1;
	case SYNSENT: 
	  fprintf(stderr, "fail! socket is in SYNSENT\n"); 
	  return -1;
	case CONNECTED:
	  fprintf(stderr, "fail! socket is in CONNECTED\n"); 
	  return -1;
	case FINWAIT: 
	  fprintf(stderr, "fail! socket is in FINWAIT\n");
	  return -1;
	default: break;
  }
  return 1;
}

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length) {
	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
  client_tcb_t* tp;
  tp = gtcb_table[sockfd];
  if(tp == NULL) {
	fprintf(stderr, "fail! socket does not exit\n");
	return -1;
  }
  seg_t syn_seg;
  switch (tp->state) {
	case CLOSED: 
	  fprintf(stderr, "fail! socket is in CLOSED\n"); 
	  return -1;
	case SYNSENT: 
	  fprintf(stderr, "fail! socket is in SYNSENT\n"); 
	  return -1;
	case CONNECTED:
	  syn_seg.header.src_port = tp->client_portNum;
	  syn_seg.header.dest_port = tp->server_portNum;
	  syn_seg.header.seq_num = tp->next_seqNum;
	  syn_seg.header.ack_num = 0;				
	  syn_seg.header.length = 0;
	  syn_seg.header.type = FIN;
	  unsigned char cnt = 0;
	  tp->state = FINWAIT;
	  for (;cnt < FIN_MAX_RETRY; cnt++) {
		sip_sendseg(gson_conn, &syn_seg);
		printf("send FIN to server %u\n", cnt);
//		usleep(FIN_TIMEOUT/1000);
		sleep(1);
		if(tp->state == CLOSED)
		  return 1;
	  }
	  tp->state = CLOSED;
	  return -1;
	case FINWAIT: 
	  fprintf(stderr, "fail! socket is in FINWAIT\n");
	  return -1;
	default: break;
  }
  return 1;
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) {
  client_tcb_t* tp = gtcb_table[sockfd];
  if (tp == NULL) {
	fprintf(stderr, "socket doesn't exist\n");
	return -1;
  }
  switch (tp->state) {
	case CLOSED: 
	  free(tp);
	  tp = NULL;
	  return 1;
	case SYNSENT: 
	  fprintf(stderr, "fail! socket is in SYNSENT\n"); 
	  return -1;
	case CONNECTED:
	  fprintf(stderr, "fail! socket is in CONNECTED\n"); 
	  return -1;
	case FINWAIT: 
	  fprintf(stderr, "fail! socket is in FINWAIT\n");
	  return -1;
	default: break;
  }
  return 1;
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg) {
  seg_t recv_seg;
  while(sip_recvseg(gson_conn, &recv_seg)) {
	int i = 0;
	client_tcb_t* tp = NULL;
	for (;i<MAX_TRANSPORT_CONNECTIONS;i++) {
	  tp = gtcb_table[i];
	  if(tp != NULL && 
		 tp->client_portNum == recv_seg.header.dest_port &&
		 tp->server_portNum ==  recv_seg.header.src_port) 
		break;
	}
	if(tp == NULL || i == MAX_TRANSPORT_CONNECTIONS)
	  continue;
	if(recv_seg.header.type == SYNACK) {
	  printf("receive SYN ACK\n");
  	  switch (tp->state) {  
		case CLOSED: break;
  		case SYNSENT:
		  tp->state = CONNECTED;
		  tp->next_seqNum = recv_seg.header.ack_num;
  		  break;
  		case CONNECTED: break;
  		case FINWAIT: break;
		default: break;
	  }
	} else if (recv_seg.header.type == FINACK) {
	  printf("receive FIN ACK\n");
  	  switch (tp->state) {
  		case CLOSED: break;
  		case SYNSENT: break;
  		case CONNECTED: break;
  		case FINWAIT:
		  if(recv_seg.header.type == FINACK)
  			tp->state = CLOSED;
  		  break;
  		default: break;
  	  }
	}
  }
  pthread_exit(NULL);
}



