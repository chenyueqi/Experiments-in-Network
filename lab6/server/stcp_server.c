#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <memory.h>
#include "stcp_server.h"
#include "../common/constants.h"

/*面向应用层的接口*/

//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

void stcp_server_init(int conn) {
  int i = 0;
  for (;i<MAX_TRANSPORT_CONNECTIONS;i++)
	gtcb_table[i] = NULL;
  gson_conn = conn;
  sem_init(&gsem, 0, 0);
  pthread_t thread;
  int rc = pthread_create(&thread, NULL, seghandler, NULL);
  if(rc) {
	fprintf(stderr, "ERROR; return code from pthread_create() of seghander is %d\n", rc);
	exit(-1);
  }
  return;
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port) {
  int i = 0;
  for (;i<MAX_TRANSPORT_CONNECTIONS && gtcb_table[i]!=NULL;i++) {}
  if (i==MAX_TRANSPORT_CONNECTIONS)
	return -1;
  gtcb_table[i] = (server_tcb_t*)malloc(sizeof(server_tcb_t));
  gtcb_table[i]->server_portNum = server_port;
  gtcb_table[i]->state = CLOSED;
  gtcb_table[i]->expect_seqNum = 0;
  gtcb_table[i]->recvBuf = (char*)malloc(sizeof(char)*RECEIVE_BUF_SIZE);
  gtcb_table[i]->usedBufLen = 0;
  gtcb_table[i]->bufMutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(gtcb_table[i]->bufMutex, NULL);
  return i;
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd) {
  server_tcb_t* tp;
  tp = gtcb_table[sockfd];
  if (tp == NULL) {
	fprintf(stderr, "socket doesn't exist\n");
	return -1;
  }
  switch (tp->state) {
	case CLOSED:
	  tp->state = LISTENING;
	  sem_wait(&gsem);
	case LISTENING:
	  return 1;
	case CONNECTED:
	  fprintf(stderr, "socket is in CONNECTED\n");
	  return -1;
	case CLOSEWAIT:
	  fprintf(stderr, "socket is in CONNECTED\n");
	  return -1;
	default:
	  break;
  }
  return 1;
}

// 接收来自STCP客户端的数据. 请回忆STCP使用的是单向传输, 数据从客户端发送到服务器端.
// 信号/控制信息(如SYN, SYNACK等)则是双向传递. 这个函数每隔RECVBUF_ROLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length) {
  server_tcb_t* tp = gtcb_table[sockfd];
  if (tp == NULL) {
	fprintf(stderr, "socket does not exist\n");
	return -1;
  }
  switch (tp->state) {
	case CLOSED: 
	  fprintf(stderr, "socket is in CLOSED\n");
	  return -1;
	case LISTENING:
	  fprintf(stderr, "socket is in LISTENING\n");
	  return -1;
	case CONNECTED:
	  while (1) {
		pthread_mutex_lock(tp->bufMutex);
		if(tp->usedBufLen >= length) {
		  memcpy(buf, tp->recvBuf, length);
		  tp->usedBufLen -= length;
		  memcpy(tp->recvBuf, tp->recvBuf + length, tp->usedBufLen);
  		  pthread_mutex_unlock(tp->bufMutex);
		  break;
		} else {
  		  pthread_mutex_unlock(tp->bufMutex);
  		  sleep(RECVBUF_POLLING_INTERVAL);
		}
	  }
	  break;
	case CLOSEWAIT:
	  fprintf(stderr, "socket is in CLOSEWAIT\n");
	  return -1;
	default:
	  break;
  }
  return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd) {
  server_tcb_t* tp = gtcb_table[sockfd];
  if(tp == NULL) {
	fprintf(stderr, "socke does not exist\n");
	return -1;
  }
  switch (tp->state) {
	case CLOSED:
	  free(gtcb_table[sockfd]->recvBuf);
	  free(gtcb_table[sockfd]->bufMutex);
	  free(gtcb_table[sockfd]);
	  gtcb_table[sockfd] = NULL;
	  return 1;
	case LISTENING:
	  fprintf(stderr, "socket is in LISTENING\n");
	  return -1;
	case CONNECTED:
	  fprintf(stderr, "socket is in CONNECTED\n");
	  return -1;
	case CLOSEWAIT:
	  fprintf(stderr, "socket is in CLOSEWAIT\n");
	  return -1;
	default: break;
  }
  return 1;
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//

void *finclock(void* arg) {
  server_tcb_t* tp = (server_tcb_t*)arg;
  sleep(CLOSEWAIT_TIMEOUT);
  tp->state = CLOSED;
  pthread_exit(NULL);
}

void *seghandler(void* arg) {
  seg_t recv_seg;
  while (sip_recvseg(gson_conn, &recv_seg)>0) {
	if(recv_seg.header.type == SYN) {
	  int i = 0;
	  server_tcb_t* tp = NULL;
	  for (; i<MAX_TRANSPORT_CONNECTIONS; i++) {  
		tp = gtcb_table[i];
		if (tp != NULL && 
			tp->server_portNum == recv_seg.header.dest_port)
		  break;
	  }
	  assert(tp != NULL);

	  switch (tp->state) {
		case CLOSED: break;
		case LISTENING: 
		  tp->state = CONNECTED;
		  sem_post(&gsem);
		case CONNECTED: 
		  tp->client_portNum = recv_seg.header.src_port;
		  tp->expect_seqNum = recv_seg.header.seq_num+1;
		  seg_t synack_seg;
		  synack_seg.header.src_port = tp->server_portNum;
		  synack_seg.header.dest_port = tp->client_portNum;
		  synack_seg.header.seq_num = 0;
		  synack_seg.header.ack_num = tp->expect_seqNum;
		  synack_seg.header.type = SYNACK;
		  sip_sendseg(gson_conn, &synack_seg);
  		  printf("send SYN ACK to client\n");
		  break;
		case CLOSEWAIT: break;
		default: break;

	  }
	} else if (recv_seg.header.type == FIN) {
	  int i = 0;
	  server_tcb_t* tp = NULL;
	  for (; i<MAX_TRANSPORT_CONNECTIONS; i++) {  
		tp = gtcb_table[i];
		if (tp != NULL && 
			tp->server_portNum == recv_seg.header.dest_port &&
			tp->client_portNum == recv_seg.header.src_port)
		  break;
	  }
	  assert(tp != NULL);

	  seg_t synack_seg;
	  switch (tp->state) {
		case CLOSED: break;
		case LISTENING: break;
		case CONNECTED:
		  tp->state = CLOSEWAIT;
		  pthread_t thread;
		  int rc = pthread_create(&thread, NULL, finclock, tp);
		  if(rc) {
			fprintf(stderr, "ERROR; return code from pthread_create() of seghander is %d\n", rc);		
			exit(-1);
		  }
		case CLOSEWAIT: 
		  synack_seg.header.src_port = tp->server_portNum;
		  synack_seg.header.dest_port = tp->client_portNum;
		  synack_seg.header.seq_num = 0;
		  synack_seg.header.ack_num = tp->expect_seqNum;
		  synack_seg.header.type = FINACK;
		  sip_sendseg(gson_conn, &synack_seg);
  		  printf("send FIN ACK to client\n");
		  break;
		default: break;
	  }
	} else if (recv_seg.header.type == DATA) {
	  int i = 0;
	  server_tcb_t* tp = NULL;
	  for (; i<MAX_TRANSPORT_CONNECTIONS; i++) {  
		tp = gtcb_table[i];
		if (tp != NULL && 
			tp->server_portNum == recv_seg.header.dest_port &&
			tp->client_portNum == recv_seg.header.src_port)
		  break;
	  }
	  assert(tp != NULL);

	  switch(tp->state) {
		case CLOSED:break;
		case LISTENING: break;
		case CONNECTED:
		  if (tp->expect_seqNum == recv_seg.header.seq_num) {
			tp->expect_seqNum += recv_seg.header.length;
			pthread_mutex_lock(tp->bufMutex);
			memcpy(tp->recvBuf + tp->usedBufLen, recv_seg.data, recv_seg.header.length);
			tp->usedBufLen += recv_seg.header.length;
			pthread_mutex_unlock(tp->bufMutex);
		  }
		  seg_t datack_seg;
		  datack_seg.header.src_port = tp->server_portNum;
		  datack_seg.header.dest_port = tp->client_portNum;
		  datack_seg.header.seq_num = 0;
		  datack_seg.header.ack_num = tp->expect_seqNum;
		  datack_seg.header.type = DATAACK;
		  sip_sendseg(gson_conn, &datack_seg);
		  printf("send DATA ACK to client\n");
		  break;
		case CLOSEWAIT: break;
		default: break;
	  }
	}
  }
  return 0;
}
