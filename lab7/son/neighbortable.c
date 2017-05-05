//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2015年

#include "neighbortable.h"

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create() {
  int nbr_num = topology_getNbrNum();
  nbr_entry_t* nbr_table = (nbr_entry_t*)malloc(sizeof(nbr_entry_t)*nbr_num);

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
	if (!strcmp(hostname, node1)) {
	  struct hostent *hostinfo;
	  hostinfo = gethostbyname(node2);
	  char **addr = hostinfo->h_addr_list;
	  nbr_table[cnt].nodeIP = *((struct in_addr*)*addr);
	  nbr_table[cnt].nodeID = topology_getNodeIDfromname(node2);
  	  nbr_table[cnt].conn = -1;
	  cnt++;
	}
	if (!strcmp(hostname, node2)) {
	  struct hostent *hostinfo;
	  hostinfo = gethostbyname(node1);
	  char **addr = hostinfo->h_addr_list;
	  nbr_table[cnt].nodeIP = *((struct in_addr*)*addr);
	  nbr_table[cnt].nodeID = topology_getNodeIDfromname(node1);
  	  nbr_table[cnt].conn = -1;
	  cnt++;
	}
  }
  fclose(f);
  return nbr_table;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt, int nbr_num) {
  int i = 0;
  for (; i < nbr_num; i++) {
	while (close(nt[i].conn) != 0) {}
  }
  free (nt);
  return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn) {
  int nbr_num = topology_getNbrNum();
  int i = 0;
  for (; i < nbr_num; i++) {
	if (nt[i].nodeID == nodeID) {
	  if (nt[i].conn == -1) {
		nt[i].conn = conn;
		return 1;
	  } else {
		return -1;
	  }
	}
  }
  return -1;
}
