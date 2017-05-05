// 文件名: common/pkt.c
// 创建日期: 2015年

#include "pkt.h"

// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn) {
  sendpkt_arg_t seg;
  seg.nextNodeID = nextNodeID;
  memcpy(&seg.pkt, pkt, sizeof(sip_pkt_t));
  char begin[2] = "!&";
  char end[2] = "!#";
  send(son_conn, begin, 2, 0);
  send(son_conn, &seg, sizeof(sendpkt_arg_t), 0);
  send(son_conn, end, 2, 0);
  return 0;
}

enum recv_fsm_states {
  PKTSTART1,
  PKTSTART2,
  PKTRECV,
  PKTSTOP1,
};

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn) {
  char tmp;
  char *buf;
  buf = (char*)malloc(sizeof(sip_pkt_t)+1);
  unsigned int cnt = 0;
  enum recv_fsm_states state = PKTSTART1;
  int n = 0;
  while ((n = recv(son_conn, &tmp, 1, 0)) > 0){
	switch (state) {
	  case PKTSTART1:
		if(tmp == '!')
		  state = PKTSTART2;
		break;
	  case PKTSTART2:
		if(tmp == '&')
		  state = PKTRECV;
		break;
	  case PKTRECV:
		buf[cnt++] = tmp;
		if(tmp == '!')
		  state = PKTSTOP1;
		break;
	  case PKTSTOP1:
		if(tmp == '#'){
		  cnt--;
		} else if (tmp == '&') {
		  fprintf(stderr, "error\n");
		  exit(-1);
		} else {
		  buf[cnt++] = tmp;
		  if (tmp != '!')
			state = PKTRECV;
		}
		break;
	  default: break;
	}
	if (tmp == '#' && state == PKTSTOP1)
	  break;
  }
  if(n < 0) {
	free(buf);
	return n;
  }
  memcpy(pkt, buf, cnt);
  free(buf);
  return cnt;
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn) {
  char tmp;
  char *buf;
  buf = (char*)malloc(sizeof(sendpkt_arg_t)+1);
  unsigned int cnt = 0;
  enum recv_fsm_states state = PKTSTART1;
  int n = 0;
  while ((n = recv(sip_conn, &tmp, 1, 0)) > 0) {
	switch (state) {
	  case PKTSTART1:
		if(tmp == '!')
		  state = PKTSTART2;
		break;
	  case PKTSTART2:
		if(tmp == '&')
		  state = PKTRECV;
		break;
	  case PKTRECV:
		buf[cnt++] = tmp;
		if(tmp == '!')
		  state = PKTSTOP1;
		break;
	  case PKTSTOP1:
		if(tmp == '#'){
		  cnt--;
		} else if (tmp == '&') {
		  fprintf(stderr, "error\n");
		  exit(-1);
		} else {
		  buf[cnt++] = tmp;
		  if (tmp != '!')
			state = PKTRECV;
		}
		break;
	  default: break;
	}
	if (tmp == '#' && state == PKTSTOP1)
	  break;
  }
  if(n < 0) {
	printf("getpktToSend failed\n");
	free(buf);
	return n;
  }
  memcpy(pkt, &(((sendpkt_arg_t*)buf)->pkt), sizeof(sip_pkt_t));
  memcpy(nextNode, &(((sendpkt_arg_t*)buf)->nextNodeID), sizeof(int));
  free(buf);
  return cnt;
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn) {
  char begin[2] = "!&";
  char end[2] = "!#";
  send(sip_conn, begin, 2, 0);
  send(sip_conn, pkt, sizeof(sip_pkt_t), 0);
  send(sip_conn, end, 2, 0);
  return 0;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn) {
  char begin[2] = "!&";
  char end[2] = "!#";
  send(conn, begin, 2, 0);
  send(conn, pkt, sizeof(sip_pkt_t), 0);
  send(conn, end, 2, 0);
  return 0;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn) {
  char tmp;
  char *buf;
  buf = (char*)malloc(sizeof(sip_pkt_t)+1);
  unsigned int cnt = 0;
  enum recv_fsm_states state = PKTSTART1;
  int n = 0;
  while ((n = recv(conn, &tmp, 1, 0)) > 0) {
	switch (state) {
	  case PKTSTART1:
		if(tmp == '!')
		  state = PKTSTART2;
		break;
	  case PKTSTART2:
		if(tmp == '&')
		  state = PKTRECV;
		break;
	  case PKTRECV:
		buf[cnt++] = tmp;
		if(tmp == '!')
		  state = PKTSTOP1;
		break;
	  case PKTSTOP1:
		if(tmp == '#'){
		  cnt--;
		} else if (tmp == '&') {
		  fprintf(stderr, "error\n");
		  exit(-1);
		} else {
		  buf[cnt++] = tmp;
		  if (tmp != '!')
			state = PKTRECV;
		}
		break;
	  default: break;
	}
	if (tmp == '#' && state == PKTSTOP1)
	  break;
  }

  if (n < 0) {
	free(buf);
	return n;
  }

  memcpy(pkt, buf, cnt);
  free(buf);
  return cnt;
}
