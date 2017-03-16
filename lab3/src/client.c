/*
   this is weather query tool is developed by Yueqi Chen,
   it will build tcp connection to the weather server to get
   required information. It is one part of network protocol 
   development experiments in Nanjing University.
   Please feel free to contact Yueqi via 
   yueqichen.0x0@gmail.com or
   yueqi.chen@psu.edu 
   whenever you have something to say.
*/

#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>

#define true 1
#define false 0
#define MAXLINE 4096


enum CLIET_STATES {
  INIT,
  WEL,
  QUERY,
  CLOSE
};

#pragma pack(1)
struct send_content {
  unsigned char stage;
  unsigned char option;
  char city_name[20];
  unsigned char day_num;
};

struct recv_content {
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

enum CLIET_STATES current_state;
int gsockfd;
//char serv_ip[16] = {"114.212.191.33"};
//char serv_ip[16] = {"127.0.0.1"};

// synthesize all view print in one function
// print out pursuant to current state
void view_print() {
  if (current_state == WEL) {
	fprintf(stdout, "Welcome to NJUCS Weather Forecast Demo Program!\n");
	fprintf(stdout, "Please input City Name in Chinese pinyin(e.g. nanjing or beijing)\n");
	fprintf(stdout, "(c)cls,(#)exit\n");
  } else if (current_state == QUERY) {
	fprintf(stdout, "Please enter the given number to query\n");
	fprintf(stdout, "1.today\n");
	fprintf(stdout, "2.three days from today\n");
	fprintf(stdout, "3.custom day by yourself\n");
	fprintf(stdout, "(r)back,(c)cls,(#)exit\n");
	fprintf(stdout, "===================================================\n");
  }
}

inline void weather_print(unsigned char weather_id) {
  switch(weather_id) {
	case 0x00: fprintf(stdout, "shower"); break;
	case 0x01: fprintf(stdout, "clear"); break;
	case 0x02: fprintf(stdout, "cloudy"); break;
	case 0x03: fprintf(stdout, "rain"); break;
	case 0x04: fprintf(stdout, "fog"); break;
	default: break;
  }
}

// send out package
int kick_out(unsigned char stage, unsigned char option, char *city_name, unsigned char day_num) {
  struct send_content new_send_content;
  new_send_content.stage = stage;
  new_send_content.option = option;
  strncpy(new_send_content.city_name, city_name, 20);
  new_send_content.day_num = day_num;
  char send_data[23]; // at most 23 bytes
  memcpy(send_data, &new_send_content, sizeof(struct send_content));
  return send(gsockfd, send_data, 23, 0);
}

// receive package
int swallow_in(struct recv_content *new_recv_content){
  char recv_data[77]; // at most 77 bytes
  recv(gsockfd, recv_data, 77, 0);
  memcpy(new_recv_content, recv_data, sizeof(struct recv_content));
  return 0;
}

int query(char *city_name) {
  current_state = QUERY;
  system("clear");
  view_print();
  char cmd[2];
  while (true) {
  	fscanf(stdin, "%s", cmd);
  	if (!strcmp(cmd,"c")) {  
	  system("clear");
  	  view_print();	
  	} else if (!strcmp(cmd, "#")) {
  	  return 0;
  	} else if (!strcmp(cmd, "r")) {  
	  return welcome();  
	} else if (!strcmp(cmd, "1")) { // today's weather
	  kick_out(0x02, 0x01, city_name, 0x1);
	  struct recv_content new_recv_content;
	  swallow_in(&new_recv_content);
	  unsigned short year = ntohs(new_recv_content.year);
	  unsigned char month = new_recv_content.month;
	  unsigned char day = new_recv_content.day;
	  // information output
	  fprintf(stdout, "City: %s  Today is: %4u/%02u/%02u  Weather information is as follows:\n",
		  	   city_name, year, month, day);
	  fprintf(stdout, "Today's Weather is: ");
	  weather_print(new_recv_content.day1_weather);
	  fprintf(stdout, ";\tTemp:%u\n", new_recv_content.day1_temp);
  	} else if (!strcmp(cmd, "2")) { // weathers in three days
	  kick_out(0x02, 0x02, city_name, 0x3);
	  struct recv_content new_recv_content;
	  swallow_in(&new_recv_content);
	  unsigned short year = ntohs(new_recv_content.year);
	  unsigned char month = new_recv_content.month;
	  unsigned char day = new_recv_content.day;
	  // information output
	  fprintf(stdout, "City: %s  Today is: %4u/%02u/%02u  Weather information is as follows:\n",
		  	   city_name, year, month, day);
	  fprintf(stdout, "The 1th day's Weather is: ");
	  weather_print(new_recv_content.day1_weather);
	  fprintf(stdout, ";\tTemp:%u\n", new_recv_content.day1_temp);
	  fprintf(stdout, "The 2nd day's Weather is: ");
	  weather_print(new_recv_content.day2_weather);
	  fprintf(stdout, ";\tTemp:%u\n", new_recv_content.day2_temp);
	  fprintf(stdout, "The 3rd day's Weather is: ");
	  weather_print(new_recv_content.day3_weather);
	  fprintf(stdout, ";\tTemp:%u\n", new_recv_content.day3_temp);
  	} else if (!strcmp(cmd, "3")) { // customized weather
	  char day_num[3];
	  while (true) {
  		fprintf(stdout, 
		  	    "Please enter the day number(below 10, e.g. 1 means today):");
		fscanf(stdin, "%s", day_num);
		if('0' <= day_num[0] && 
			day_num[0] <= '9' && 
			day_num[1] == '\0') {
		  kick_out(0x02, 0x01, city_name, 0x3);
		  struct recv_content new_recv_content;
		  swallow_in(&new_recv_content);
		  unsigned short year = ntohs(new_recv_content.year);
		  unsigned char month = new_recv_content.month;
		  unsigned char day = new_recv_content.day;
		  // information output
		  fprintf(stdout, "City: %s  Today is: %4u/%02u/%02u  Weather information is as follows:\n",
		  	   city_name, year, month, day);
		  if (day_num[0] == '1') {	  
			fprintf(stdout, "Today's Weather is: ");
	  		weather_print(new_recv_content.day1_weather);
	  		fprintf(stdout, ";\tTemp:%u\n", new_recv_content.day1_temp);
		  } else {
	  		fprintf(stdout, "The %cth day's Weather is: ", day_num[0]);
	  		weather_print(new_recv_content.day1_weather);
	  		fprintf(stdout, ";  Temp:%u\n", new_recv_content.day1_temp);
		  }
		  break;
		} else {
		  fprintf(stdout, "input error\n");		  
		}
	  }
	} else {  
	  fprintf(stdout, "input error!\n");
  	}
  }
  return 0;
}

int welcome() {
  current_state = WEL;
  system("clear");
  view_print();
  char city_name[20];
  while (true) {
	fscanf(stdin, "%s", city_name);
  	if (!strcmp(city_name,"c")) {
	  system("clear");
  	  view_print();
	} else if (!strcmp(city_name, "#")) {
  	  return 0;
	} else {
	  kick_out(0x01, 0x00, city_name, 0x0);
	  struct recv_content new_recv_content;
	  swallow_in(&new_recv_content);
	  if(new_recv_content.res == 0x01) {
		return query(city_name);
	  } else { // if res == 0x02
		fprintf(stdout, 
				"Sorry, Server does not have weather information for city %s!\n",
				city_name);
		view_print();
		continue;
	  }
	}
  }
  return 0;
}

int main(int argc, char **argv) {
  current_state =  INIT;
  struct sockaddr_in servaddr;
  gsockfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(argv[1]);
  servaddr.sin_port =  htons(4321);
  if (!connect(gsockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
	welcome();
  } else {
	fprintf(stderr, "Can Not Connect To Server %s!\n", argv[1]);
	return 0;
  }
  while (close(gsockfd)!=0){}
  return 0;
}
