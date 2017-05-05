//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 
//
//创建日期: 2015年

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 20

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t* gnt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int gsip_conn; 
int gmy_id;
int gnbr_num;

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
void* waitNbrs(void* arg) {
  int cnt = 0;
  int i = 0;
  for (; i < gnbr_num; i++) {
	if (gnt[i].nodeID > gmy_id)
	  cnt++;
  }

  struct sockaddr_in myaddr, nbraddr;
  socklen_t nbrlen;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(CONNECTION_PORT);
  bind(listenfd, (struct sockaddr*)&myaddr, sizeof(myaddr));
  listen(listenfd, gnbr_num);
  printf("waiting %d nodes to connect...\n", cnt);

  while (cnt > 0) {
	int tmp_conn = accept(listenfd, (struct sockaddr*)&nbraddr, &nbrlen);
	int nbr_id = topology_getNodeIDfromip(&nbraddr.sin_addr);
	printf("accept id: %d\n", nbr_id);
	int i = 0;
	for (; i < gnbr_num; i++) {
	  if(gnt[i].nodeID == nbr_id) {
		assert(gnt[i].conn == -1);
		printf("connection established, neighbour id: %d\n", nbr_id);
		gnt[i].conn = tmp_conn;
		cnt--;
	  }
	}
  }
  pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
  int i = 0;
  for (;i < gnbr_num; i++) {
	if  (gnt[i].nodeID < gmy_id) {
	  printf("connection to id: %d\n", gnt[i].nodeID);
	  struct sockaddr_in nbraddr;
	  int nbr_conn = socket(AF_INET, SOCK_STREAM, 0);
	  memset(&nbraddr, 0,sizeof(nbraddr));
	  nbraddr.sin_family = AF_INET;
	  nbraddr.sin_addr.s_addr = inet_addr(inet_ntoa(gnt[i].nodeIP));
	  printf("ip: %s\n", inet_ntoa(gnt[i].nodeIP));
	  nbraddr.sin_port = htons(CONNECTION_PORT);
	  if (!connect(nbr_conn, (struct sockaddr*)&nbraddr, sizeof(nbraddr))) {
		gnt[i].conn = nbr_conn;
		printf("connected\n");
	  } else {
		fprintf(stderr, "connection failed\n");
		return -1;
	  }
	}
  }
  return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) {
  int idx = *(int*)arg;
  printf("index: %d\n", idx);
  int conn = gnt[idx].conn;
  sip_pkt_t pkt;
  int n = 0;
  printf("started listen thread: %d\n", gnt[idx].nodeID);
  while ((n = recvpkt(&pkt, conn)) > 0) {
	printf("son received a packet from neighbor %d\n", gnt[idx].nodeID);
	if (gsip_conn != -1) {
	  forwardpktToSIP(&pkt, gsip_conn);
	}
  }

  if (n < 0) {
  	printf("ended listen thread\n");
	pthread_exit(NULL);
  }
  return 0;
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
  struct sockaddr_in sonaddr, sipaddr;
  socklen_t siplen;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  sonaddr.sin_family = AF_INET;
  sonaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  sonaddr.sin_port = htons(SON_PORT);
  bind(listenfd, (struct sockaddr*)&sonaddr, sizeof(sonaddr));
  listen(listenfd, 1);
  while(1) {
	printf("waiting for connection from sip\n");
	gsip_conn = accept(listenfd, (struct sockaddr*)&sipaddr, &siplen);
	printf("received connection from sip\n");
	sendpkt_arg_t sip_arg;
	while (getpktToSend(&sip_arg.pkt, &sip_arg.nextNodeID, gsip_conn) > 0) {
	  printf("received pkt from sip\n");
  	  sip_arg.pkt.header.src_nodeID = gmy_id;
  	  if (sip_arg.nextNodeID == BROADCAST_NODEID) {
  		int i = 0;
  		for (; i < gnbr_num; i++)
  		  sendpkt(&sip_arg.pkt, gnt[i].conn);
  	  }
	}
	gsip_conn = -1;
  }
  return ;
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
  fprintf(stderr, "in son stop\n");
  nt_destroy(gnt, gnbr_num);
  if (gsip_conn != -1)
  	while (close(gsip_conn) != 0) {}
  exit(0);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",gmy_id = topology_getMyNodeID());	

	//创建一个邻居表
	gnt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	gsip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	gnbr_num = topology_getNbrNum();
	int i;
	for(i=0;i<gnbr_num;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,gnt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	
	//创建线程监听所有邻居
	for(i=0;i<gnbr_num;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
