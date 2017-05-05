//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2015年

#include "topology.h"

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) {
  struct hostent *hostinfo;
  hostinfo = gethostbyname(hostname);
  if (!hostinfo) {
	fprintf(stderr, "cannot get info for host: %s\n", hostname);
	return -1;
  }
  if (hostinfo->h_addrtype != AF_INET) {
	fprintf(stderr, "not an IP host!\n");
	return -1;
  }
  char **addrs = hostinfo->h_addr_list;
  char *ip = inet_ntoa(*(struct in_addr*)*addrs);
  int i = 0;
  while (i < 3) {
	if(*ip == '.')
	  i++;
	ip++;
  }
  return atoi(ip);
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr) {
  char *ip = inet_ntoa(*addr);
  int i = 0;
  while (i < 3) {
	if(*ip == '.')
	  i++;
	ip++;
  }
  return atoi(ip);
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID() {
  char hostname[256];
  gethostname(hostname, 255);
  return topology_getNodeIDfromname(hostname);
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum() {
  char hostname[256];
  gethostname(hostname, 255);
  FILE* f;
  f = fopen("topology/topology.dat", "r");
  assert(f!=NULL);
  char node1[256];
  char node2[256];
  int cost = 0;
  int cnt = 0;
  while(!feof(f)) {
	fscanf(f, "%s", node1);
	if(feof(f))
	  break;
	fscanf(f, "%s", node2);
	fscanf(f, "%d", &cost);
	if(!strcmp(hostname, node1))
	  cnt++;
	if(!strcmp(hostname, node2))
	  cnt++;
  }
  fclose(f);
  return cnt;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum() { 
  FILE* f;
  f = fopen("topology/topology.dat", "r");
  assert(f!=NULL);
  char node[20][256];
  int cnt = 0;
  while(!feof(f)) {
	char tmp1[256];
	char tmp2[256];
	fscanf(f, "%s", tmp1);
	if(feof(f))
	  break;
	int cost = 0;
	fscanf(f, "%s", tmp2);
	fscanf(f, "%d", &cost);
	if(cnt == 0) {
	  strcpy(node[0], tmp1);
	  strcpy(node[1], tmp2);
	  cnt = 2;
	} else {
	  int i = 0;
	  int flag1 = 0;
	  int flag2 = 0;
	  for (;i < cnt; i++) {
		if (!strcmp(node[i], tmp1))
		  flag1 = 1;
		if (!strcmp(node[i],tmp2))
		  flag2 = 1;
	  }
	  if (flag1 == 0)
		strcpy(node[cnt++], tmp1);
	  if (flag2 == 0)
		strcpy(node[cnt++], tmp2);
	}
  }
  fclose(f);
  return cnt;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray() {
  int nodenum = topology_getNodeNum();
  int* node_id = (int*)malloc(sizeof(int)*nodenum);
  FILE* f;
  f = fopen("topology/topology.dat", "r");
  assert(f!=NULL);
  int cnt = 0;
  while(!feof(f)) {
	char tmp1[256];
	char tmp2[256];
	fscanf(f, "%s", tmp1);
	if(feof(f))
	  break;
	int cost = 0;
	fscanf(f, "%s", tmp2);
	fscanf(f, "%d", &cost);
	if(cnt == 0) {
	  node_id[0] = topology_getNodeIDfromname(tmp1);
	  node_id[1] = topology_getNodeIDfromname(tmp2);
	  cnt = 2;
	} else {
	  int i = 0;
	  int flag1 = 0;
	  int flag2 = 0;
	  for (;i < cnt; i++) {
		if (node_id[i] == topology_getNodeIDfromname(tmp1))
		  flag1 = 1;
		if (node_id[i] == topology_getNodeIDfromname(tmp2))
		  flag2 = 1;
	  }
	  if (flag1 == 0)
  		node_id[cnt++] = topology_getNodeIDfromname(tmp1);
	  if (flag2 == 0)
  		node_id[cnt++] = topology_getNodeIDfromname(tmp2);
	}
  }
  fclose(f);
  return node_id;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray() {
  return 0;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
  return 0;
}
