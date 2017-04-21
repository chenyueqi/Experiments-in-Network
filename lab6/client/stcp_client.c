#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <memory.h>
#include "stcp_client.h"

//
//  �����������ṩ��ÿ���������õ�ԭ�Ͷ����ϸ��˵��, ����Щֻ��ָ���Ե�, ����ȫ���Ը����Լ����뷨����ƴ���.
//
//  ע��: ��ʵ����Щ����ʱ, ����Ҫ����FSM�����п��ܵ�״̬, �����ʹ��switch�����ʵ��.
//
//  Ŀ��: ������������Ʋ�ʵ������ĺ���ԭ��.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL.  
// ��������ص�����TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, �ñ�����Ϊsip_sendseg��sip_recvseg���������.
// ���, �����������seghandler�߳�����������STCP��. �ͻ���ֻ��һ��seghandler.
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


// ����������ҿͻ���TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��. ����, TCB state������ΪCLOSED���ͻ��˶˿ڱ�����Ϊ�������ò���client_port. 
// TCB������Ŀ��������Ӧ��Ϊ�ͻ��˵����׽���ID�������������, �����ڱ�ʶ�ͻ��˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
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

// ��������������ӷ�����. �����׽���ID�ͷ������Ķ˿ں���Ϊ�������. �׽���ID�����ҵ�TCB��Ŀ.  
// �����������TCB�ķ������˿ں�,  Ȼ��ʹ��sip_sendseg()����һ��SYN�θ�������.  
// �ڷ�����SYN��֮��, һ����ʱ��������. �����SYNSEG_TIMEOUTʱ��֮��û���յ�SYNACK, SYN �ν����ش�. 
// ����յ���, �ͷ���1. ����, ����ش�SYN�Ĵ�������SYN_MAX_RETRY, �ͽ�stateת����CLOSED, ������-1.
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


// �������ݸ�STCP������. �������ʹ���׽���ID�ҵ�TCB���е���Ŀ. 
// Ȼ����ʹ���ṩ�����ݴ���segBuf, �������ӵ����ͻ�����������. 
// ������ͻ������ڲ�������֮ǰΪ��, һ����Ϊsendbuf_timer���߳̾ͻ�����. 
// ÿ��SENDBUF_ROLLING_INTERVALʱ���ѯ���ͻ������Լ���Ƿ��г�ʱ�¼�����.
// ��������ڳɹ�ʱ����1�����򷵻�-1. 
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

// ����������ڶϿ���������������. �����׽���ID��Ϊ�������. �׽���ID�����ҵ�TCB���е���Ŀ.  
// �����������FIN�θ�������. �ڷ���FIN֮��, state��ת����FINWAIT, ������һ����ʱ��.
// ��������ճ�ʱ֮ǰstateת����CLOSED, �����FINACK�ѱ��ɹ�����. ����, ����ھ���FIN_MAX_RETRY�γ���֮��,
// state��ȻΪFINWAIT, state��ת����CLOSED, ������-1.
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

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
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


// ������stcp_client_init()�������߳�. �������������Է������Ľ����. 
// seghandler�����Ϊһ������sip_recvseg()������ѭ��. ���sip_recvseg()ʧ��, ��˵���ص����������ѹر�,
// �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���. ��鿴�ͻ���FSM���˽����ϸ��.
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

// ����̳߳�����ѯ���ͻ������Դ�����ʱ�¼�. ������ͻ������ǿ�, ��Ӧһֱ����.
// ���(��ǰʱ�� - ��һ���ѷ��͵�δ��ȷ�϶εķ���ʱ��) > DATA_TIMEOUT, �ͷ���һ�γ�ʱ�¼�.
// ����ʱ�¼�����ʱ, ���·��������ѷ��͵�δ��ȷ�϶�. �����ͻ�����Ϊ��ʱ, ����߳̽���ֹ.
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
