#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <memory.h>
#include "stcp_client.h"

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

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
  gtcb_table[i]->bufMutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(gtcb_table[i]->bufMutex, NULL);
  gtcb_table[i]->sendBufHead = NULL;
  gtcb_table[i]->sendBufunSent = NULL;
  gtcb_table[i]->sendBufTail = NULL;
  gtcb_table[i]->unAck_segNum = 0;
  return i;
}

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
	  syn_seg.header.rcv_win = GBN_WINDOW;
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


// 发送数据给STCP服务器. 这个函数使用套接字ID找到TCB表中的条目. 
// 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中. 
// 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动. 
// 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生.
// 这个函数在成功时返回1，否则返回-1. 
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_client_send(int sockfd, void* data, unsigned int length)
{
  client_tcb_t* tp;
  tp = gtcb_table[sockfd];
  if (tp == NULL) {
	fprintf(stderr, "fail! socket does not exist\n");
	return -1;
  }

  if (tp->state != CONNECTED) {
	fprintf(stderr, "fail! socket hasn't connected");
	return -1;
  }
  int segNum = 0;

  switch (tp->state) {
	case CLOSED:
	  fprintf(stderr, "socket is in CLOSED\n");
	  break;
	case SYNSENT:
	  fprintf(stderr, "socket is in SYNSENT\n");
	  break;
	case CONNECTED:
	  segNum = length/MAX_SEG_LEN;
	  if (length%MAX_SEG_LEN)
		segNum++;
	  int i = 0;
	  char* data_to_send = (char*)data;
	  for (;i < segNum; i++) {
		segBuf_t* new_seg = (segBuf_t*)malloc(sizeof(segBuf_t));
		assert(new_seg != NULL);
		memset(new_seg, 0, sizeof(segBuf_t));
		new_seg->seg.header.src_port = tp->client_portNum;
		new_seg->seg.header.dest_port = tp->server_portNum;
		new_seg->seg.header.seq_num = tp->next_seqNum;
		if (length%MAX_SEG_LEN!=0 && i==segNum-1)
		  new_seg->seg.header.length = length % MAX_SEG_LEN;
		else
		  new_seg->seg.header.length = MAX_SEG_LEN;
		tp->next_seqNum += new_seg->seg.header.length;
		new_seg->seg.header.ack_num = 0;
		new_seg->seg.header.type = DATA;
		new_seg->seg.header.rcv_win = GBN_WINDOW;
		memcpy(new_seg->seg.data, &data_to_send[i*MAX_SEG_LEN], new_seg->seg.header.length);
		new_seg->next = NULL;

		if(tp->sendBufHead == NULL) {
		  tp->sendBufHead = new_seg;
		  tp->sendBufunSent = new_seg;
		  tp->sendBufTail = new_seg;
		} else {
		  tp->sendBufTail->next = new_seg;
		  tp->sendBufTail = new_seg;
		}
	  }
	  break;
	case FINWAIT:
	  fprintf(stderr, "socket is in FINWAIT\n");
	  break;
	default: break;
  }
  

  pthread_t thread;
  int rc = pthread_create(&thread, NULL, sendBuf_timer, (void*)tp);
  if(rc) {
	fprintf(stderr, "ERROR; return code from pthread_create() of seghander is %d\n", rc);
	exit(-1);
  }

  while(1) {
	pthread_mutex_lock(tp->bufMutex);
	if(tp->sendBufHead == NULL) {
	  pthread_mutex_unlock(tp->bufMutex);
	  break;
	} 
	pthread_mutex_unlock(tp->bufMutex);
	sleep(1);
  }

  return 0;
}

// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
  client_tcb_t* tp;
  tp = gtcb_table[sockfd];
  if(tp == NULL) {
	fprintf(stderr, "fail! socket does not exist\n");
	return -1;
  }

  while(1) {
	pthread_mutex_lock(tp->bufMutex);
	if(tp->sendBufHead == NULL) {
	  pthread_mutex_unlock(tp->bufMutex);
	  break;
	}
	pthread_mutex_unlock(tp->bufMutex);
	sleep(1);
  }
  
  seg_t fin_seg;
  switch (tp->state) {
	case CLOSED: 
	  fprintf(stderr, "fail! socket is in CLOSED\n"); 
	  return -1;
	case SYNSENT: 
	  fprintf(stderr, "fail! socket is in SYNSENT\n"); 
	  return -1;
	case CONNECTED:
	  fin_seg.header.src_port = tp->client_portNum;
	  fin_seg.header.dest_port = tp->server_portNum;
	  fin_seg.header.seq_num = tp->next_seqNum;
	  fin_seg.header.ack_num = 0;				
	  fin_seg.header.length = 0;
	  fin_seg.header.type = FIN;
	  fin_seg.header.rcv_win = GBN_WINDOW;
	  unsigned char cnt = 0;
	  tp->state = FINWAIT;
	  for (;cnt < FIN_MAX_RETRY; cnt++) {
		sip_sendseg(gson_conn, &fin_seg);
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
	  free(tp->bufMutex);
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
	} else if (recv_seg.header.type == DATAACK) {
	  printf("receive DATA ACK\n");
	  switch (tp->state) {
		case CLOSED: break;
		case SYNSENT: break;
		case CONNECTED: 
		  pthread_mutex_lock(tp->bufMutex);
		  while (tp->sendBufHead != NULL &&
			  	 tp->sendBufHead != tp->sendBufunSent &&
  				 tp->sendBufHead->seg.header.seq_num < recv_seg.header.ack_num) {
			segBuf_t* segbuf_p = tp->sendBufHead;
			tp->sendBufHead = segbuf_p->next;
			free(segbuf_p);
			tp->unAck_segNum--;
		  }

		  if(tp->sendBufHead == NULL) {
			tp->sendBufunSent = tp->sendBufTail = NULL;
			tp->unAck_segNum = 0;
		  }
		  pthread_mutex_unlock(tp->bufMutex);
		  break;
		case FINWAIT: break;
		default: break;
	  }
	}
  }
  pthread_exit(NULL);
}

// 这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
// 如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
// 当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void* sendBuf_timer(void* clienttcb) {
  client_tcb_t* tp = (client_tcb_t*)clienttcb;
  if(tp == NULL) {
	fprintf(stderr, "socket does not exist\n");
	exit(-1);
  }
  while(1) {
	pthread_mutex_lock(tp->bufMutex);
	if(tp->sendBufHead == NULL) {
	  assert(tp->unAck_segNum == 0);
  	  pthread_mutex_unlock(tp->bufMutex);
	  pthread_exit(NULL);
	} else {
	  segBuf_t* seg_p = tp->sendBufHead;
	  while(seg_p != tp->sendBufunSent) {
		sip_sendseg(gson_conn, &seg_p->seg);
		seg_p = seg_p->next;
	  }

	  for (; tp->unAck_segNum < GBN_WINDOW && tp->sendBufunSent != NULL; 
		  tp->unAck_segNum++, tp->sendBufunSent = tp->sendBufunSent->next) {
		sip_sendseg(gson_conn, &tp->sendBufunSent->seg);
	  }  
	}
	pthread_mutex_unlock(tp->bufMutex);
	usleep(DATA_TIMEOUT/1000);
  }
  return;
}
