/*
   this is the client of lab04, we use
   semaphore for synchronization between threads

   feel free to contact Yueqi or Zibo if you have anything
   to say
*/

#include"common.h"

enum CLIENT_STATES {
  INIT,
  WELCOM,
  CLIENT_ONLINE,
  CLIENT_WAR,
};

enum CLIENT_STATES gcurrent_state;
int gsockfd;
char guser_name[20];
char gadversary_name[20];
// semaphore for concurrency between
// listen thread and main thread
sem_t gsem;

void welcome();
void online();
void war();

// embeded in main thread,
// if you are in leisure, you can extend it 
// to another isolated thread. But since it is 
// a network experiment, we just shrink it to 
// several code of lines
void view_print() {
  system("clear");
  switch (gcurrent_state) {
	case WELCOM: 
	 fprintf(stdout, "Welcome to stone knife cloth game platform\n");
	 fprintf(stdout, "1. sign up\n");
	 fprintf(stdout, "2. sign in\n");
	 fprintf(stdout, "3. exit\n");
	 break;
	case CLIENT_ONLINE:
	 fprintf(stdout, "%s, Welcome abroad!\n", guser_name);
	 fprintf(stdout, "1. friends list\n");
	 fprintf(stdout, "2. credit list\n");
	 fprintf(stdout, "3. begin war\n");
	 fprintf(stdout, "4. begin chatting\n");
	 fprintf(stdout, "5. sign out\n");
	 break;
	case CLIENT_WAR:
	 fprintf(stdout, "%s vs %s will begin in 5 seconds\n", 
		 	 guser_name, gadversary_name);
	 fprintf(stdout, "1. stone\n");
	 fprintf(stdout, "2. knife\n");
	 fprintf(stdout, "3. cloth\n");
	 sleep(5);
	 fprintf(stdout, "War begins!\n");
	 break;
  }
}

// TODO  other functions for processing
// different packets
// shall be done by Zibo

void sign_up_in_response_proc(struct sign_up_in_packet_response *response_packet) {
  if (response_packet->up_in == 0x00) { // sign up
	if(response_packet->valid_or_not == 0x00) // valid
	  fprintf(stdout, "user %s created\n", response_packet->user_name);
	else // invalid
	  fprintf(stdout, "user %s exists\n", response_packet->user_name);
  } else { // sign in
	if(response_packet->valid_or_not == 0x00) { // valid
	  strncpy(guser_name, (char*)response_packet->user_name, 20);
	  gcurrent_state = CLIENT_ONLINE;
	} else if(response_packet->valid_or_not == 0x01) { // invalid
	  fprintf(stdout, "user %s does not exist or password is incorrect\n", 
		  	  response_packet->user_name);
	} else {
	  fprintf(stdout, "user %s has already been online\n", 
		  	  response_packet->user_name);
	}
  }
  sem_post(&gsem);
  return ;
}

void friends_credit_list_response_proc(struct friends_credit_list_response *new_packet) {
  if(new_packet->credit_or_not == 0x00) {
	fprintf(stdout, "Friends List: total %u friends on the platform\n", new_packet->num);
  	for (int i = 0; i < new_packet->num; i++) {
  	  fprintf(stdout, "%s\t", new_packet->list[i].user_name);
  	  switch(new_packet->list[i].state) {
  		case OFFLINE: break; // shall never come to this branch
  		case ONLINE: fprintf(stdout, "ONLINE\n"); break;
  		case WAR: fprintf(stdout, "In WAR\n"); break;
  		default: break;
	  }
  	}
  } else {
	fprintf(stdout, "Credit List:\n");
	for (int i = 0; i < new_packet->num; i++) {
	  fprintf(stdout, "%s\t%u\n", new_packet->list[i].user_name, 
		  	  new_packet->list[i].credit);
	}
  }
  sem_post(&gsem);
  return;
}

void friends_state_change_proc(struct friends_state_change_info *info_packet) {
  setbuf(stdout, NULL);
  if(info_packet->friend_num == 0x01) {
  	fprintf(stdout, "\nYour friends %s is ", info_packet->friend_name);  
	if (info_packet->state == ONLINE)
  	  fprintf(stdout, "ONLINE\n>> ");  
	else if (info_packet->state == OFFLINE)
  	  fprintf(stdout, "OFFLINE\n>> ");
  } else {
  	fprintf(stdout, "\nYour friends %s and %s is IN WAR\n>> ", info_packet->friend_name,
		    info_packet->friend_name2);  
  }
  return;
}

void war_request_b_proc(struct war_request_b* new_packet) {
  setbuf(stdout, NULL);
  strncpy(gadversary_name, (char*)new_packet->request_name, 20);
  fprintf(stdout, "%s wants to have a war with you", new_packet->request_name);
  fprintf(stdout, "'y' to accept, 'n' to reject\n");
}

void war_response_a_proc(struct war_response_a* new_packet) {
  setbuf(stdout, NULL);
  if(new_packet->answer == 0x00) {
	fprintf(stdout, "%s is offline\n", new_packet->adversary_name);
  } else if (new_packet->answer == 0x01) {
	fprintf(stdout, "%s is in war\n", new_packet->adversary_name);
  } else if (new_packet->answer == 0x02) {
	fprintf(stdout, "%s refuses your request\n", new_packet->adversary_name);
  } else if (new_packet->answer == 0x03) {
	fprintf(stdout, "%s accepts your request\n", new_packet->adversary_name);
	strncpy(gadversary_name, (char*)new_packet->adversary_name, 20);
	gcurrent_state = CLIENT_WAR;
  }
  sem_post(&gsem);
  return;
}

void turn_result_proc(struct turn_result* new_packet) {
  setbuf(stdout, NULL);
  fprintf(stdout, "Turn %u\n", new_packet->turn);
  fprintf(stdout, "You ");
  switch(new_packet->res) {
	case 0x00: fprintf(stdout, "WIN\n"); break;
	case 0x01: fprintf(stdout, "LOSE\n"); break;
	case 0x02: fprintf(stdout, "DRAW\n"); break;
	case 0x03: fprintf(stdout, "LOSE Due to overtime\n"); break;
	default: break;
  }
  fprintf(stdout, "Your blood: %u\n", new_packet->blood);
  fprintf(stdout, "Your adversary %s blood: %u\n", 
	  	  gadversary_name, new_packet->adversary_blood);
  switch(new_packet->final_res) {
	case 0x00: break;
	case 0x01: 
		fprintf(stdout, "You have won the war !\n");
		fprintf(stdout, "Get 3 credits !\n");
		gcurrent_state = CLIENT_ONLINE;
		sleep(5);
		sem_post(&gsem);
		return;
	case 0x02:
		fprintf(stdout, "You have lost the war !\n");
		gcurrent_state = CLIENT_ONLINE;
		sleep(5);
		sem_post(&gsem);
		return;
  }
  setbuf(stdout, NULL);
  fprintf(stdout, "Turn %u\n", new_packet->turn+1);
  fprintf(stdout, "You have 10 seconds to pick up your choice\n");
  fprintf(stdout, ">> ");
  if(new_packet->res != 0x03)
  	sem_post(&gsem);
  return;
}

void chat_proc(struct chat_packet* new_packet) {
  setbuf(stdout, NULL);
  if (new_packet->valid_or_not == 0x00){ 
  	fprintf(stdout, "New Message from friend %s\n", (char*)new_packet->friend_name);
  	fprintf(stdout, "\n--\n%s--", (char*)new_packet->message);
  }
  else
  	fprintf(stdout, "Your friend is offline or in war.");
  fprintf(stdout, "\n>> ");
  return;
}

void sign_out_response_proc(struct sign_out_response *new_packet) {
  strncpy(guser_name, "", 20);
  gcurrent_state = WELCOM;
  sem_post(&gsem);
}

void* listen_thread(void*) {
  int n = 0;
  struct general_packet new_packet;
  while((n = recv(gsockfd, &new_packet, sizeof(struct general_packet), 0)) > 0) {
	switch(new_packet.func) {  
	  case 0x01: 
  		sign_up_in_response_proc((struct sign_up_in_packet_response*)&new_packet); 
		break;
	  case 0x03:
		friends_credit_list_response_proc((struct friends_credit_list_response*)&new_packet);
		break;
	  case 0x04: 
		friends_state_change_proc((struct friends_state_change_info*)&new_packet); 
		break;
	  case 0x06:
		war_request_b_proc((struct war_request_b*)&new_packet);
		break;
	  case 0x08:
		war_response_a_proc((struct war_response_a*)&new_packet);
		break;
	  case 0x0a:
		turn_result_proc((struct turn_result*)&new_packet);
		break;
	  case 0x0b:
		chat_proc((struct chat_packet*)&new_packet);
		break;
	  case 0x0d:
		sign_out_response_proc((struct sign_out_response*)&new_packet);
		break;
	  default: break;
	}
  }
}

void welcome() {
  gcurrent_state = WELCOM;
  view_print();
  char cmd[3];
  while (gcurrent_state == WELCOM) {
	fprintf(stdout, ">> ");
  	fscanf(stdin, "%s", cmd);
  	if (!strcmp(cmd,"1")) { // sign up
	  char user_name[20];
	  char passwd[20];
	  fprintf(stdout, "new user name:\n");
	  fscanf(stdin, "%s", user_name);
	  fprintf(stdout, "new password:\n");
	  fscanf(stdin, "%s", passwd);
	  struct general_packet new_packet;
	  struct sign_up_in_packet_request* new_request;
	  new_request = (struct sign_up_in_packet_request*)&new_packet;
	  new_request->func = 0x00;
	  strncpy((char*)new_request->user_name, (char*)user_name, 20);
	  new_request->up_in = 0x00;
	  strncpy((char*)new_request->passwd, (char*)passwd, 20);
	  send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
  	  sem_wait(&gsem); // wait for listen thread
  	} else if (!strcmp(cmd, "2")) { // sign in
	  unsigned char user_name[20];
	  unsigned char passwd[20];
	  fprintf(stdout, "user name:\n");
	  fscanf(stdin, "%s", user_name);
	  fprintf(stdout, "password:\n");
	  fscanf(stdin, "%s", passwd);
	  struct general_packet new_packet;
	  struct sign_up_in_packet_request* new_request;
	  new_request = (struct sign_up_in_packet_request*)&new_packet;
	  new_request->func = 0x00;
	  strncpy((char*)new_request->user_name, (char*)user_name, 20);
	  new_request->up_in = 0x01;
	  strncpy((char*)new_request->passwd, (char*)passwd, 20);
	  send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
  	  sem_wait(&gsem); // wait for listen thread
	} else if (!strcmp(cmd, "3")) { // exit
	  exit(0);
	} else {
  	  fprintf(stdout, "input error!\n");
  	}
  }
  return;
}

void online() {
  gcurrent_state = CLIENT_ONLINE;
  view_print();
  char cmd[3];
  setbuf(stdin, NULL);
  setbuf(stdout, NULL);
  while (gcurrent_state == CLIENT_ONLINE) {
	fprintf(stdout, ">> ");
  	fscanf(stdin, "%s", cmd);
  	if (!strcmp(cmd,"1")) { // friends list
	  struct general_packet new_packet;
	  struct friends_credit_list_request* new_request;
	  new_request = (struct friends_credit_list_request*)&new_packet;
	  new_request->func = 0x02;
	  strncpy((char*)new_request->user_name, (char*)guser_name, 20);
	  new_request->credit_or_not = 0x00;
	  send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
  	  sem_wait(&gsem);
	} else if (!strcmp(cmd, "2")) { // credit list
	  struct general_packet new_packet;
	  struct friends_credit_list_request* new_request;
	  new_request = (struct friends_credit_list_request*)&new_packet;
	  new_request->func = 0x02;
	  strncpy((char*)new_request->user_name, (char*)guser_name, 20);
	  new_request->credit_or_not = 0x01;
	  send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
  	  sem_wait(&gsem);
	} else if (!strcmp(cmd, "3")) { // begin war
	  fprintf(stdout, "Who you want to play game with?\n>> ");
	  char adversary_name[20];
	  fscanf(stdin, "%s", adversary_name);
	  struct general_packet new_packet;
	  struct a_war_request* new_request;
	  new_request = (struct a_war_request*)&new_packet;
	  new_request->func = 0x05;
	  strncpy((char*)new_request->user_name, (char*)guser_name, 20);
	  strncpy((char*)new_request->adversary_name, adversary_name, 20);
	  new_request->random_or_not = 0x01;
	  send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
  	  sem_wait(&gsem);
	} else if (!strcmp(cmd, "4")) { // begin chat
	  fprintf(stdout, "Who you want to chat to?\n>> ");
	  char friend_name[20];
	  fscanf(stdin, "%s", friend_name);
	  fprintf(stdout, "What the message?\n>> ");
	  char message[128];
	  setbuf(stdin, NULL);
	  fgets(message, 128, stdin);
	  setbuf(stdin, NULL);
	  struct general_packet new_packet;
	  struct chat_packet* new_request;
	  new_request = (struct chat_packet*)&new_packet;
	  new_request->func = 0x0b;
	  strncpy((char*)new_request->user_name, (char*)guser_name, 20);
	  strncpy((char*)new_request->friend_name, friend_name, 20);
	  strncpy((char*)new_request->message, message, 128);
	  send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
	} else if (!strcmp(cmd, "5")) { // sign out
	  struct general_packet new_packet;
	  struct sign_out_request* new_request;
	  new_request = (struct sign_out_request*)&new_packet;
	  new_request->func = 0x0c;
	  strncpy((char*)new_request->user_name, (char*)guser_name, 20);
	  send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
  	  sem_wait(&gsem);
	} else if (!strcmp(cmd, "y")) { // accept war request
	  struct general_packet new_packet;
	  struct b_war_response* new_response;
	  new_response = (struct b_war_response*)&new_packet;
	  new_response->func = 0x07;
	  strncpy((char*)new_response->user_name, guser_name, 20);
	  strncpy((char*)new_response->request_name, gadversary_name, 20);
	  new_response->yes_or_not = 0x00;
	  send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
	  gcurrent_state = CLIENT_WAR;
	} else if (!strcmp(cmd, "n")) { // reject war request
	  struct general_packet new_packet;
	  struct b_war_response* new_response;
	  new_response = (struct b_war_response*)&new_packet;
	  new_response->func = 0x07;
	  strncpy((char*)new_response->user_name, guser_name, 20);
	  strncpy((char*)new_response->request_name, gadversary_name, 20);
	  new_response->yes_or_not = 0x01;
	  send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
	}
  }
  return;
}

void war() {
  gcurrent_state = CLIENT_WAR;
  view_print();
  char cmd[3];
  setbuf(stdout, NULL);
  fprintf(stdout, "Turn 1\n");
  fprintf(stdout, "You have 10 seconds to pick up your choice\n>> ");
  while (gcurrent_state == CLIENT_WAR) {
  	setbuf(stdin, NULL);
  	setbuf(stdout, NULL);
  	fscanf(stdin, "%s", cmd);
	struct general_packet new_packet;
	struct stone_knife_cloth* new_pick;
	new_pick = (struct stone_knife_cloth*) &new_packet;
	new_pick->func = 0x09;
	strncpy((char*)new_pick->user_name, guser_name, 20);
  	if (!strcmp(cmd,"1"))  // stone
	  new_pick->s_k_c = 0x00;
	else if (!strcmp(cmd, "2")) // knife
	  new_pick->s_k_c = 0x01;
	else if (!strcmp(cmd, "3")) // cloth
	  new_pick->s_k_c = 0x02;
	send(gsockfd, &new_packet, sizeof(struct general_packet), 0);
	fprintf(stdout, "waiting....\n");
	sem_wait(&gsem);
  }
  return;
}

// state machine
void main_loop() {
  while(true) {
	switch (gcurrent_state) {
	  case WELCOM: welcome();break;
	  case CLIENT_ONLINE: online();break;
	  case CLIENT_WAR: war(); break;
	  default: break;
	}
  }
}

int main() {
  gcurrent_state = INIT;
  struct sockaddr_in servaddr;
  gsockfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  servaddr.sin_port = htons(4321);
  sem_init(&gsem, 0,0);
  if(!connect(gsockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
	pthread_t thread;
	int rc = pthread_create(&thread, NULL, listen_thread, NULL);
	gcurrent_state = WELCOM;
	main_loop();
  } else {
	fprintf(stderr, "Can Not Connect To Server\n");
	return 0;
  }

  while (close(gsockfd)!=0) {}
  return 0;
}
