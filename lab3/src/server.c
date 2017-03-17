/*
   this is the server source code of the weather query service by Yueqi
   The server runs parallelly (if you have several cores) by using 
   sub process to for handling request

   I made use of director's handout 
   (especially how to realize a parallel server) to achieve it.
   Feel free to contact me if you have anything to say

   My email address is yueqichen.0x0@gmail.com
   OR				   yueqi.chen@psu.edu
*/

#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<time.h>

#define LISTENQ 8
#define true 1
#define false 0
typedef int bool;

char city_list[4][20] = {"beijing",  
  						 "nanjing", 
						 "shanghai", 
						 "guangzhou"};
#pragma pack(1)
struct recv_content {
  unsigned char stage;
  unsigned char option;
  char city_name[20];
  unsigned char day_num;
};

struct send_content {
  unsigned char res; // result of query
  unsigned char useless;
  char city_name[20];
  unsigned short year;
  unsigned char month;
  unsigned char day;
  unsigned char unit_num;
  unsigned char day1_weather;
  unsigned char day1_temp;
  unsigned char day2_weather;
  unsigned char day2_temp;
  unsigned char day3_weather;
  unsigned char day3_temp;
};
#pragma pack()

void kick_out(int connfd, struct send_content* new_send_content) {
  char send_data[77];
  memset(send_data, 0, sizeof(char)*77);
  memcpy(send_data, new_send_content, sizeof(struct send_content));
  send(connfd, send_data, 77, 0);
  return;
}

void city_verify(int connfd, char* city_name) {
  struct send_content new_send_content;
  memset(&new_send_content, 0, sizeof(struct send_content));
  int i = 0;
  bool flag = false;
  for (;i < 4; i++) {
	if(!strcmp(city_list[i], city_name)) {
	  flag = true;
	  break;
	}
  }
  if(flag)
  	new_send_content.res = 0x01;
  else
  	new_send_content.res = 0x02;
  new_send_content.useless = 0x00;
  strcpy(new_send_content.city_name, city_name);
  kick_out(connfd, &new_send_content);
  return;
}

void weather_gen(int connfd, struct recv_content* new_recv_content) {
  struct send_content new_send_content;
  memset(&new_send_content, 0, sizeof(struct send_content));
  new_send_content.res = 0x03;
  strcpy(new_send_content.city_name, new_recv_content->city_name);

  time_t rawtime;
  struct tm* timeinfo;
  rawtime = time(NULL);
  timeinfo = localtime(&rawtime);
  new_send_content.year = htons((short)timeinfo->tm_year + 1900); // begin from 1900
  new_send_content.month = (unsigned char)timeinfo->tm_mon + 1; // 0-11
  new_send_content.day = (unsigned char)timeinfo->tm_mday;

  if(new_recv_content->option == 0x01) { // generate one day's weather
	if(new_recv_content->day_num == 0x09) {
	  new_send_content.res = 0x04;
  	  new_send_content.useless = 0x41;
  	  new_send_content.unit_num = 0x1;
	} else {
  	  new_send_content.useless = 0x41;
  	  new_send_content.unit_num = 0x1;
  	  new_send_content.day1_weather = rand()%5;
  	  new_send_content.day1_temp = rand()%40;
	}
	kick_out(connfd, &new_send_content);
  } else if(new_recv_content->option == 0x02) { // generate three days' weathers
  	new_send_content.useless = 0x42;
	new_send_content.unit_num = 0x3;

	new_send_content.day1_weather = rand()%5;
	new_send_content.day1_temp = rand()%40;
	new_send_content.day2_weather = rand()%5;
	new_send_content.day2_temp = rand()%40;
	new_send_content.day3_weather = rand()%5;
	new_send_content.day3_temp = rand()%40;

	kick_out(connfd, &new_send_content);
  } else { // shall never come here
	return;
  }
}

void serve(int connfd) {
  int n = 0;
  unsigned char buf[100];
  while((n = recv(connfd, buf, 100, 0)) > 0) {
	struct recv_content new_recv_content;
	memcpy(&new_recv_content, buf, sizeof(struct recv_content));
	if (new_recv_content.stage == 0x1)
	  city_verify(connfd, new_recv_content.city_name);
	else if (new_recv_content.stage == 0x2) 
	  weather_gen(connfd, &new_recv_content);
	else // shall never come here
  	  fprintf(stderr, "error !\n");
  }
  if(n < 0) 
	fprintf(stderr, "error !\n");
  exit(0);
}


int main() {
  int listenfd;
  pid_t childpid;
  socklen_t clilen;
  struct sockaddr_in cliaddr, servaddr;
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(4321);

  bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  listen(listenfd, LISTENQ);
  fprintf(stdout, "Server is running, waiting for connections ..\n");

  while (true) {
	clilen = sizeof(cliaddr);
	int connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
	fprintf(stdout, "%s\n", "Received request ...");
	if ((childpid = fork()) == 0) {
	  printf("%s\n\n", "Child process created for dealing with client request");
	  close(listenfd); // sub-process shall not have listen socket
	  serve(connfd);
	}
	close(connfd);
  }
  return 0;
}
