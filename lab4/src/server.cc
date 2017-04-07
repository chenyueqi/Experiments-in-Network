/*
   this is the server of lab04 by Yueqi
   Feel free to contact Yueqi if you have anything to say

   Yueqichen.0x0@gmail.com
*/


#include"common.h"
#include<map>
#include<string>
#include<vector>
#include<algorithm>
using namespace std;

#define LISTENQ 8
#define SIG_BROAD __SIGRTMIN+1

struct user {
  enum PLAYER_STATES state;
  char passwd[20];
  unsigned char credit;
  pthread_t tid;
  int connfd;
  string room_name;
};

struct war_room {
  char player_a[20];
  char player_b[20];
  char a_blood;
  char b_blood;
  int a_sockfd;
  int b_sockfd;
  pthread_t tid;
  // yes 0x00
  // no 0x01
  unsigned char war_response;
  sem_t war_room_sem;
  struct stone_knife_cloth skc1;
  struct stone_knife_cloth skc2;
  bool skc1_recved;
  bool skc2_recved;
};

// anonymous struct for broadcast data,
// which is used globally by all client threads
static struct {
  unsigned user_num;
  char user_name[20];
  char user_name2[20];
  enum PLAYER_STATES state;
} broad_info;

map<string, user> user_table;
pthread_t gbroad_thread;
sem_t gbroad_sem;
map<string, war_room> war_room_table;

int cmp(const pair<string, unsigned char> &x, const pair<string, unsigned char> &y) {
  return x.second > y.second;
}
vector<pair<string, char> > credit_vec;

void* war_thread(void* user_name);

void sign_up_in(int connfd, struct sign_up_in_packet_request* new_packet,
				string& client_user_name, enum PLAYER_STATES& client_state) {
  chdir("./user_info");
  if (new_packet->up_in == 0x00) { // sign up
	FILE* file = fopen((char*)new_packet->user_name, "r");
	if (file !=  NULL) { // Oh, No! user exists
	  fclose(file);
	  struct general_packet response_packet;
	  struct sign_up_in_packet_response* new_response;
	  new_response = (struct sign_up_in_packet_response*)&response_packet;
	  strncpy((char*)new_response->user_name, (char*)new_packet->user_name, 20);
	  new_response->func = 0x01;
	  new_response->up_in = 0x00;
	  new_response->valid_or_not = 0x01;
	  send(connfd, &response_packet, sizeof(struct general_packet), 0);
	} else {
	  file = fopen((char*)new_packet->user_name, "w");
	  fprintf(file, "%s\t0", new_packet->passwd);
	  fclose(file);
	  struct general_packet response_packet;
	  struct sign_up_in_packet_response* new_response;
	  new_response = (struct sign_up_in_packet_response*)&response_packet;
	  strncpy((char*)new_response->user_name, (char*)new_packet->user_name, 20);
	  new_response->func = 0x01;
	  new_response->up_in = 0x00;
	  new_response->valid_or_not = 0x00;
	  send(connfd, &response_packet, sizeof(struct general_packet), 0);
	}
  } else if (new_packet->up_in == 0x01) { // sign in
	FILE* file = fopen((char*)new_packet->user_name, "r");
	if (file != NULL) { // Great! user exists
	  string user_name((char*)new_packet->user_name);
	  map<string, user>::iterator iter = user_table.find(user_name);
	  if (iter != user_table.end()) { // if this user has already been online
  		struct general_packet response_packet;
  		struct sign_up_in_packet_response* new_response;
  		new_response = (struct sign_up_in_packet_response*)&response_packet;  
		strncpy((char*)new_response->user_name, (char*)new_packet->user_name, 20);
  		new_response->func = 0x01;
  		new_response->up_in = 0x01;
  		new_response->valid_or_not = 0x02;
  		send(connfd, &response_packet, sizeof(struct general_packet), 0);
	  } else {
  		char passwd[20];
  		unsigned int credit;
  		fscanf(file, "%s\t%u", passwd, &credit);
  		fclose(file);

  		struct general_packet response_packet;
  		struct sign_up_in_packet_response* new_response;
  		new_response = (struct sign_up_in_packet_response*)&response_packet;
  		strncpy((char*)new_response->user_name, (char*)new_packet->user_name, 20);
  		new_response->func = 0x01;
  		new_response->up_in = 0x01;
  		if (!strcmp(passwd, (char*)new_packet->passwd)) {
  		  new_response->valid_or_not = 0x00;
   		  struct user new_user;
  		  new_user.state = ONLINE;
  		  strncpy(new_user.passwd, passwd, 20);
  		  new_user.credit = credit;
  		  new_user.tid = pthread_self();
  		  new_user.connfd = connfd;
  		  string user_name((char*)response_packet.user_name);
  		  user_table.insert(pair<string, user>(user_name, new_user));
		  credit_vec.push_back(make_pair(user_name, credit));

  		  client_user_name = (char*)new_packet->user_name;
		  client_state = ONLINE;

  		  strncpy(broad_info.user_name, (char*)response_packet.user_name, 20);
  		  broad_info.state = ONLINE;
  		  sem_post(&gbroad_sem);
  		} else {
  		  new_response->valid_or_not = 0x01;
  		} 
		send(connfd, &response_packet, sizeof(struct general_packet), 0);
	  }
	} else {
	  struct general_packet response_packet;
	  struct sign_up_in_packet_response* new_response;
	  new_response = (struct sign_up_in_packet_response*)&response_packet;
	  strncpy((char*)new_response->user_name, (char*)new_packet->user_name, 20);
	  new_response->func = 0x01;
	  new_response->up_in = 0x01;
	  new_response->valid_or_not = 0x01;
	  send(connfd, &response_packet, sizeof(struct general_packet), 0);
	}
  }
  chdir("./..");
  return;
}

void friends_credit_list(int connfd, struct friends_credit_list_request* new_packet) {
  struct general_packet response_packet;
  struct friends_credit_list_response* new_response;
  new_response = (struct friends_credit_list_response*)&response_packet;
  new_response->func = 0x03;
  strncpy((char*)new_response->user_name, (char*)new_packet->user_name, 20);
  if (new_packet->credit_or_not == 0x00) { // user list
	new_response->credit_or_not = 0x00;
	new_response->num = 0;
	for (map<string, user>::iterator iter = user_table.begin();
		 (iter != user_table.end()) && (new_response->num < 10); 
		 iter++, new_response->num++) {
	  strncpy((char*)new_response->list[new_response->num].user_name,
		      iter->first.c_str(),20); 
	  new_response->list[new_response->num].state = iter->second.state;
	  new_response->list[new_response->num].credit = iter->second.credit;
	}
  } else { // credit list
	new_response->credit_or_not = 0x01;
	new_response->num = 0;
	sort(credit_vec.begin(), credit_vec.end(), cmp);
	for (vector<pair<string, char> >::iterator iter = credit_vec.begin();
		(iter != credit_vec.end()) && (new_response->num < 10);
		iter++, new_response->num++) {
	  strncpy((char*)new_response->list[new_response->num].user_name, 
		  	  iter->first.c_str(), 20);
	  new_response->list[new_response->num].credit = iter->second;
	}
  }
  send(connfd, &response_packet, sizeof(struct general_packet), 0);
  return;
}

void a_war_request_proc(int connfd, struct a_war_request* new_packet) {
  // just ignore random case
  string adversary_name((char*)new_packet->adversary_name);
  map<string , user>::iterator iter = user_table.find(adversary_name);
  if (iter == user_table.end()) { // adversary is offline
	struct general_packet response_packet;
	struct war_response_a* new_response;
	new_response = (struct war_response_a*)&response_packet;
	new_response->func = 0x08;
	strncpy((char*)new_response->user_name, (char*)new_packet->user_name, 20);
	strncpy((char*)new_response->adversary_name, (char*)new_packet->adversary_name, 20);
	new_response->answer = 0x00;
  	send(connfd, &response_packet, sizeof(struct general_packet), 0);
  } else {
	if (iter->second.state == WAR) { // adversary is in war
  	  struct general_packet response_packet;
  	  struct war_response_a* new_response;
  	  new_response = (struct war_response_a*)&response_packet;
  	  new_response->func = 0x08;
  	  strncpy((char*)new_response->user_name, (char*)new_packet->user_name, 20);
  	  strncpy((char*)new_response->adversary_name, (char*)new_packet->adversary_name, 20);
  	  new_response->answer = 0x01;
  	  send(connfd, &response_packet, sizeof(struct general_packet), 0);
	} else {
	  struct war_room new_war;
	  sem_init(&new_war.war_room_sem, 0,0);
	  strncpy(new_war.player_a, (char*)new_packet->user_name, 20);
	  strncpy(new_war.player_b, (char*)new_packet->adversary_name, 20);
	  new_war.a_blood = init_blood;
	  new_war.b_blood = init_blood;
	  new_war.a_sockfd = connfd;
	  new_war.b_sockfd = iter->second.connfd;
	  new_war.war_response = 0x00;
	  string user_name((char*)new_packet->user_name);
	  int rc = pthread_create(&new_war.tid, NULL, war_thread, (void *)new_packet->user_name);
  	  if(rc) {
  		fprintf(stdout, "ERROR: return code from pthread_create is %d\n", rc);
  		exit(-1);
  	  }
	  war_room_table.insert(pair<string, war_room>(user_name, new_war));
  	}
  }
  return;
}

void b_war_response_proc(struct b_war_response* new_packet) {
  string adversary_name((char*)new_packet->request_name);
  map<string, war_room>::iterator iter = war_room_table.find(adversary_name);
  if (iter == war_room_table.end()) {
	fprintf(stdout, "no room for adversary %s\n", new_packet->request_name);
	return;
  }
  if (new_packet->yes_or_not == 0x00) // yes, begin war
	iter->second.war_response = 0x00;
  else // no
	iter->second.war_response = 0x01;
  sem_post(&iter->second.war_room_sem);
  return;
}

void skc_proc(struct stone_knife_cloth* new_packet) {
  string user_name((char*)new_packet->user_name);
  map<string, user>::iterator user_iter = user_table.find(user_name);
  if(user_name == user_iter->second.room_name) {
	map<string, war_room>::iterator war_iter = war_room_table.find(user_iter->second.room_name);
	war_iter->second.skc1 = *new_packet;
	war_iter->second.skc1_recved = true;
  } else {
	map<string, war_room>::iterator war_iter = war_room_table.find(user_iter->second.room_name);
	war_iter->second.skc2 = *new_packet;
	war_iter->second.skc2_recved = true;
  }
  return;
}

void chat(struct chat_packet* new_packet) {
  string friend_name((char*)new_packet->friend_name);
  map<string, user>::iterator iter = user_table.find(friend_name);
  if ((iter == user_table.end()) || 
	  (iter->second.state == WAR)) { // friend is not online or in war
  	struct general_packet new_response;
	struct chat_packet* new_chat_packet = (struct chat_packet*)&new_response;
  	new_chat_packet->func = 0x0b;
  	new_chat_packet->valid_or_not = 0x01;
  	strncpy((char*)new_chat_packet->user_name, (char*)new_packet->friend_name, 20);
  	strncpy((char*)new_chat_packet->friend_name, (char*)new_packet->user_name, 20);
  	strncpy((char*)new_chat_packet->message, (char*)new_packet->message, 128);
  	string user_name((char*)new_packet->user_name);
  	map<string, user>::iterator iter_user = user_table.find(user_name);
	send(iter_user->second.connfd, &new_response, sizeof(struct general_packet), 0);
  } else {
  	struct general_packet new_response;
	struct chat_packet* new_chat_packet = (struct chat_packet*)&new_response;
  	new_chat_packet->func = 0x0b;
  	new_chat_packet->valid_or_not = 0x00;
  	strncpy((char*)new_chat_packet->user_name, (char*)new_packet->friend_name, 20);
  	strncpy((char*)new_chat_packet->friend_name, (char*)new_packet->user_name, 20);
  	strncpy((char*)new_chat_packet->message, (char*)new_packet->message, 128);
	send(iter->second.connfd, &new_response, sizeof(struct general_packet), 0);
  }
  return;
}

// sign out 
void sign_out(int connfd, struct sign_out_request* new_packet,
			  string& client_user_name, enum PLAYER_STATES& client_state) {
  string user_name((char*)new_packet->user_name);
  map<string, user>::iterator iter = user_table.find(user_name);
  if(iter == user_table.end())
	return;
  chdir("./user_info");
  FILE* file = fopen((char*)new_packet->user_name, "w");
  fprintf(file, "%s\t%u", iter->second.passwd, iter->second.credit);
  fclose(file);
  chdir("./..");
  user_table.erase(iter);
  for (vector<pair<string, char> >::iterator credit_iter = credit_vec.begin();
	   credit_iter != credit_vec.end(); credit_iter++) {
	if(credit_iter->first == user_name) {
	  credit_vec.erase(credit_iter);
	  break;
	}
  }

  struct general_packet response_packet;
  struct sign_out_response* new_response;
  new_response = (struct sign_out_response*)&response_packet;
  strncpy((char*)new_response->user_name, (char*)new_packet->user_name, 20);
  new_response->func = 0x0d;
  send(connfd, &response_packet, sizeof(struct general_packet), 0);
  client_state = OFFLINE;
  client_user_name = "";
  strncpy(broad_info.user_name, (char*)response_packet.user_name, 20);
  broad_info.state = OFFLINE;
  broad_info.user_num = 0x01;
  sem_post(&gbroad_sem);
  return;
}

void war_turn_proc(string room_name) {
  map<string, war_room>::iterator room_iter = war_room_table.find(room_name);
  unsigned char turn_num = 1;
  while(room_iter->second.a_blood > 0 &&
	    room_iter->second.b_blood > 0) {
	struct general_packet response1, response2;
	struct turn_result* res1_p;
	struct turn_result* res2_p;
	res1_p = (struct turn_result*)&response1;
	res2_p = (struct turn_result*)&response2;
	res1_p->func = 0x0a;
	res2_p->func = 0x0a;
	res1_p->turn = turn_num;
	res2_p->turn = turn_num;
	strncpy((char*)res1_p->user_name, room_iter->second.player_a, 20);
	strncpy((char*)res2_p->user_name, room_iter->second.player_b, 20);

	sleep(15);
	if(room_iter->second.skc1_recved && room_iter->second.skc2_recved) { // normal
	  struct stone_knife_cloth* skc_packet1 = &room_iter->second.skc1;
	  struct stone_knife_cloth* skc_packet2 = &room_iter->second.skc2;
	  if(skc_packet1->s_k_c == skc_packet2->s_k_c) { // draw
  		res1_p->res = 0x02;
  		res1_p->blood = room_iter->second.a_blood;
  		res1_p->adversary_blood = room_iter->second.b_blood;

  		res2_p->res = 0x02;
  		res2_p->blood = room_iter->second.b_blood;
  		res2_p->adversary_blood = room_iter->second.a_blood;
	  } else if ((skc_packet1->s_k_c < skc_packet2->s_k_c) || 
		  		 (skc_packet1->s_k_c == 0x02 && skc_packet2->s_k_c == 0x00)) { // a win
  		room_iter->second.b_blood -= blood_cut;
  		res1_p->res = 0x00;
  		res1_p->blood = room_iter->second.a_blood;
  		res1_p->adversary_blood = room_iter->second.b_blood;

  		res2_p->res = 0x01;
  		res2_p->blood = room_iter->second.b_blood;
  		res2_p->adversary_blood = room_iter->second.a_blood;
	  } else { // b wins
  		room_iter->second.a_blood -= blood_cut;
  		res1_p->res = 0x01;
  		res1_p->blood = room_iter->second.a_blood;
  		res1_p->adversary_blood = room_iter->second.b_blood;

  		res2_p->res = 0x00;
  		res2_p->blood = room_iter->second.b_blood;
  		res2_p->adversary_blood = room_iter->second.a_blood;
	  }

	} else if(room_iter->second.skc1_recved && !room_iter->second.skc2_recved) { // b over time
	  room_iter->second.b_blood -= blood_cut;

	  res1_p->res = 0x00;
	  res1_p->blood = room_iter->second.a_blood;
	  res1_p->adversary_blood = room_iter->second.b_blood;

	  res2_p->res = 0x03;
	  res2_p->blood = room_iter->second.b_blood;
	  res2_p->adversary_blood = room_iter->second.a_blood;
	} else if(!room_iter->second.skc1_recved && room_iter->second.skc2_recved ) { // a over time
	  room_iter->second.a_blood -= blood_cut;

	  res1_p->res = 0x03;
	  res1_p->blood = room_iter->second.a_blood;
	  res1_p->adversary_blood = room_iter->second.b_blood;

	  res2_p->res = 0x00;
	  res2_p->blood = room_iter->second.b_blood;
	  res2_p->adversary_blood = room_iter->second.a_blood;

	} else { // both over time
	  room_iter->second.a_blood -= blood_cut;
	  room_iter->second.b_blood -= blood_cut;

	  res1_p->res = 0x03;
	  res1_p->blood = room_iter->second.a_blood;
	  res1_p->adversary_blood = room_iter->second.b_blood;

	  res2_p->res = 0x03;
	  res2_p->blood = room_iter->second.b_blood;
	  res2_p->adversary_blood = room_iter->second.a_blood;
	}
	turn_num++;
	if (res1_p->blood <= 0 && 
		res2_p->blood <= 0 &&
		res1_p->blood == res2_p->blood) {
	  res1_p->final_res = 0x03; // final draw
	  res2_p->final_res = 0x03; // final draw
	  war_room_table.erase(room_iter);
	} else  if (res1_p->blood <= 0 &&
			   res2_p->blood > 0) {
	  res1_p->final_res = 0x02; // final lose
	  res2_p->final_res = 0x01; // final win
	  string player_a(room_iter->second.player_a);
	  string player_b(room_iter->second.player_b);
	  map<string, user>::iterator user_iter = user_table.find(player_a);
	  user_iter->second.credit -= delta_credit;
	  user_iter = user_table.find(player_b);
	  user_iter->second.credit += delta_credit;
	  war_room_table.erase(room_iter);
	} else if (res1_p->blood > 0 &&
			   res2_p->blood <= 0){
	  res1_p->final_res = 0x01; // final win
	  res2_p->final_res = 0x02; // final lose
	  string player_a(room_iter->second.player_a);
	  string player_b(room_iter->second.player_b);
	  map<string, user>::iterator user_iter = user_table.find(player_a);
	  user_iter->second.credit += delta_credit;
	  user_iter = user_table.find(player_b);
	  user_iter->second.credit -= delta_credit;
	  war_room_table.erase(room_iter);
	} else {
	  res1_p->final_res = 0x00; // not final
	  res2_p->final_res = 0x00; // not final
	}
	room_iter->second.skc1_recved = false;
	room_iter->second.skc2_recved = false;
  	send(room_iter->second.a_sockfd, &response1, sizeof(struct general_packet), 0);
  	send(room_iter->second.b_sockfd, &response2, sizeof(struct general_packet), 0);
  }
  return;
}

void* war_thread(void* name) {
  string user_name((char*)name);
  map<string, war_room>::iterator iter = war_room_table.find(user_name);
  struct general_packet request_packet;
  struct war_request_b* new_request;
  new_request = (struct war_request_b*)&request_packet;
  new_request->func = 0x06;
  strncpy((char*)new_request->user_name, iter->second.player_b, 20);
  strncpy((char*)new_request->request_name, iter->second.player_a, 20);
  send(iter->second.b_sockfd, &request_packet, sizeof(struct general_packet), 0);
  sem_wait(&iter->second.war_room_sem);
  if (iter->second.war_response == 0x00) { // accepted
	struct general_packet response_packet;
	struct war_response_a* new_response;
	new_response = (struct war_response_a*)&response_packet;
	new_response->func = 0x08;
	strncpy((char*)new_response->user_name, iter->second.player_a, 20);
	strncpy((char*)new_response->adversary_name, iter->second.player_b, 20);
	new_response->answer = 0x03;
  	send(iter->second.a_sockfd, &response_packet, sizeof(struct general_packet), 0);

	string player_a(iter->second.player_a);
	string player_b(iter->second.player_b);
	map<string, user>::iterator user_iter = user_table.find(player_a);
	user_iter->second.state = WAR;
	user_iter->second.room_name = iter->first;
	user_iter = user_table.find(player_b);
	user_iter->second.state = WAR;
	user_iter->second.room_name = iter->first;

  	strncpy(broad_info.user_name, iter->second.player_a, 20);
  	strncpy(broad_info.user_name2, iter->second.player_b, 20);
	broad_info.user_num = 0x02;
  	broad_info.state = WAR;
  	sem_post(&gbroad_sem);
	iter->second.skc1_recved = false;
	iter->second.skc2_recved = false;
	war_turn_proc(user_name);

	user_iter->second.state = ONLINE;
	user_iter = user_table.find(player_a);
	user_iter->second.state = ONLINE;
  } else if (iter->second.war_response == 0x01) { // rejected
	struct general_packet response_packet;
	struct war_response_a* new_response;
	new_response = (struct war_response_a*)&response_packet;
	new_response->func = 0x08;
	strncpy((char*)new_response->user_name, iter->second.player_a, 20);
	strncpy((char*)new_response->adversary_name, iter->second.player_b, 20);
	new_response->answer = 0x02;
  	send(iter->second.a_sockfd, &response_packet, sizeof(struct general_packet), 0);
  }
  pthread_exit(NULL);
}

void* client_thread(void* sockfd) {
  int connfd = (int)(long)sockfd;
  int n = 0;
  enum PLAYER_STATES user_state =  OFFLINE;
  string user_name;
  struct general_packet new_packet;
  while ((n = recv(connfd, &new_packet, sizeof(struct general_packet), 0)) > 0) {
	switch(new_packet.func) {
	  case 0x00: sign_up_in(connfd, 
					 		(struct sign_up_in_packet_request*)&new_packet,
							user_name, user_state);
				 break;
	  case 0x02: friends_credit_list(connfd,
					 				 (struct friends_credit_list_request*)&new_packet);
				 break;
	  case 0x05: a_war_request_proc(connfd, (struct a_war_request*)&new_packet);
				 break;
	  case 0x07: b_war_response_proc((struct b_war_response*)&new_packet);
				 break;
	  case 0x09: skc_proc((struct stone_knife_cloth*)&new_packet);
				 break;
	  case 0x0b: chat((struct chat_packet*)&new_packet);
				 break;
	  case 0x0c: sign_out(connfd, (struct sign_out_request*)&new_packet,
						  user_name, user_state);
				 break;
	  default: break;
	}
  }
  if (n < 0)
	fprintf(stderr, "Error !\n");
  if(user_state == ONLINE) {  
	chdir("./user_info");
	FILE* file = fopen(user_name.c_str(), "w");
	map<string, user>::iterator iter = user_table.find(user_name);
	fprintf(file, "%s\t%u", 
			iter->second.passwd,
			iter->second.credit);
	fclose(file);
	chdir("./..");
  }
  fprintf(stdout, "Connect closed\n");
  pthread_exit(NULL);
}

void* broadcast_thread(void*) {
  while(true) {
	sem_wait(&gbroad_sem);
	for (map<string, user>::iterator iter = user_table.begin();
		 iter != user_table.end(); iter++) {
	  if(iter->second.state == ONLINE) {
		struct general_packet response_packet;
		struct friends_state_change_info* new_response;	
		new_response = (struct friends_state_change_info*)&response_packet;
		new_response->func = 0x04;
		strncpy((char*)new_response->friend_name, broad_info.user_name, 20);
		if (broad_info.user_num == 0x02) {
		  strncpy((char*)new_response->friend_name2, broad_info.user_name2, 20);
		  new_response->friend_num = 0x02;
		} else {
		  new_response->friend_num = 0x01;
		}
		new_response->state = broad_info.state;
		strncpy((char*)new_response->user_name, iter->first.c_str(), 20);
		send(iter->second.connfd, &response_packet, sizeof(struct general_packet), 0);
	  }
	}
  }
}

int main() {
  // broadcast thread and semaphore for it
  sem_init(&gbroad_sem, 0,0);
  int rc = pthread_create(&gbroad_thread, NULL, broadcast_thread, NULL);
  if (rc) {
	fprintf(stdout, "ERROR: return code from pthread_create is %d\n", rc);
	exit(-1);
  }

  // build up listen socket and accept any connection request
  int listenfd;
  struct sockaddr_in servaddr;
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr =  htonl(INADDR_ANY);
  servaddr.sin_port = htons(4321);

  bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  listen(listenfd, LISTENQ);
  fprintf(stdout, "Server is running , waiting for connections ... \n");
  
  while (true) {
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	int connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
	fprintf(stdout, "Received Request .. \n");
	// build up a client thread for every connection request
	pthread_t thread;
	int rc = pthread_create(&thread, NULL, client_thread, (void *)(long)connfd);
	if(rc) {
	  fprintf(stdout, "ERROR: return code from pthread_create is %d\n", rc);
	  exit(-1);
	}
  }

  close(listenfd);
  pthread_exit(NULL);
}
