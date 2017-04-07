/*
   this head file is written by Yueqi for interfere between
   client server, in which packet format is defined.

   Noted that data struct used in server for store user's in-
   formation is not included.

   Feel free to contact Yueqi (yueqichen.0x0@gmail) if you have
   anything to say.
   03/26/2017
*/

#ifndef _COMMON_H_
#define _COMMON_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<unistd.h>
#include<memory.h>
#include<semaphore.h>

#define true 1
#define false 0

const char init_blood = 40;
const char blood_cut = 20;
const char delta_credit = 3;

#pragma pack(1)

struct general_packet {
  unsigned char func; // for request and response number
  unsigned char user_name[20];
  unsigned char others[300];
};

struct sign_up_in_packet_request {
  unsigned char func; // 0x00
  unsigned char user_name[20];
  // up: 0x00
  // in: 0x01
  unsigned char up_in;
  unsigned char passwd[20];
};

struct sign_up_in_packet_response {
  unsigned char func; // 0x01
  unsigned char user_name[20];
  // up: 0x00
  // in: 0x01
  unsigned char up_in;
  // valid: 0x00
  // invalid: 0x01
  // has been online: 0x02
  unsigned char valid_or_not;
};

struct friends_credit_list_request {
  unsigned char func; // 0x02
  unsigned char user_name[20];
  // 0x00 for original friends request, 
  // 0x01 for credits list
  unsigned char credit_or_not;
};

enum PLAYER_STATES {
  OFFLINE,
  ONLINE,
  WAR,
};

struct friends_list_entry {
  unsigned char user_name[20];
  enum PLAYER_STATES state; 
  unsigned char credit;
};

struct friends_credit_list_response {
  unsigned char func; // 0x03
  unsigned char user_name[20];
  // 0x00 for original friends request, 
  // 0x01 for credits list
  unsigned char credit_or_not;
  unsigned char num;
  struct friends_list_entry list[10]; 
};

struct friends_state_change_info {
  unsigned char func; // 0x04
  unsigned char user_name[20];
  unsigned char friend_num; // if state = WAR, =0x02
  unsigned char friend_name[20];
  unsigned char friend_name2[20];
  enum PLAYER_STATES state;
};

// if a wants a war
struct a_war_request {
  unsigned char func;// 0x05
  unsigned char user_name[20];
  // 0x00 for random allocate
  // 0x01 for pick adversary
  unsigned char random_or_not;
  unsigned char adversary_name[20];
};

// server asks b if accept a war from request
struct war_request_b {
  unsigned char func; // 0x06
  unsigned char user_name[20];
  unsigned char request_name[20];
};

// b answer server if accepting 
struct b_war_response {
  unsigned char func; // 0x07
  unsigned char user_name[20];
  unsigned char request_name[20];
  // yes: 0x00
  // no:  0x01
  unsigned char yes_or_not;
};

struct war_response_a {
  unsigned char func; // 0x08
  unsigned char user_name[20];
  unsigned char adversary_name[20];
  // adversary offline: 0x00
  // adversary in war: 0x01
  // adversary says no: 0x02
  // adversary says yes: 0x03
  unsigned char answer;
};

struct stone_knife_cloth {
  unsigned char func; // 0x09
  unsigned char user_name[20];
  unsigned char turn;
  // 0x00 for stone
  // 0x01 for knife 
  // 0x02 for cloth
  unsigned char s_k_c; 
}; 

struct turn_result {
  unsigned char func; // 0x0a
  unsigned char user_name[20];
  // -1 to begin war
  char turn;
  // 0x00 win 
  // 0x01 lose 
  // 0x02 draw
  // 0x03 lose due to overtime
  unsigned char res;
  char blood;
  char adversary_blood;
  // not final: 0x00
  // final win: 0x01
  // final lose: 0x02
  // final draw: 0x03
  unsigned char final_res; 
};

struct chat_packet {
  unsigned char func; // 0x0b
  // 0x00 for valid
  // 0x01 if friend is offline or in war
  unsigned char valid_or_not;
  unsigned user_name[20];
  unsigned friend_name[20];
  unsigned message[128];
};

struct sign_out_request {
  unsigned char func; // 0x0c
  unsigned user_name[20];
};

struct sign_out_response {
  unsigned char func; // 0x0d
  unsigned user_name[20];
};
#pragma pack()

#endif

