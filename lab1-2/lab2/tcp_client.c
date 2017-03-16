/*
	 comments are provided by Yueqi Chen (Yueqichen.0x0@gmail.com)
	 feel free to contact Yueqi Chen is you have anything to say 
	 about the comments and the tcp client codes.
*/


#include "sysinclude.h"

extern void tcp_DiscardPkt(char* pBuffer, int type);

// called by tcp_kick(), receive tcp segment and encapsulat it for IP protocol
extern void tcp_sendIpPkt(unsigned char* pData, UINT16 len, 
						  unsigned int  srcAddr, unsigned int dstAddr, 
						  UINT8	ttl);
// passively receive IP packet from IP layer
// return value shall be the length of tcp segment
extern int waitIpPacket(char *pBuffer, int timeout);

extern unsigned int getIpv4Address();

extern unsigned int getServerIpv4Address();

#define INPUT 0
#define OUTPUT 1

#define NOT_READY 0
#define READY 1

#define DATA_NOT_ACKED 0
#define DATA_ACKED 1

#define NOT_USED 0
#define USED 1

#define MAX_TCP_CONNECTIONS 5

#define INPUT_SEG 0
#define OUTPUT_SEG 1

typedef int STATE;

int gLocalPort = 2007;
int gRemotePort = 2006;
int gSeqNum = 1234;
int gAckNum = 0;

// enumination of TCP state
enum TCP_STATES
{
	CLOSED,
	SYN_SENT,
	ESTABLISHED,
	FIN_WAIT1,
	FIN_WAIT2,
	TIME_WAIT,
};

struct MyTcpSeg
{
	unsigned short src_port;
	unsigned short dst_port;
	unsigned int seq_num;
	unsigned int ack_num;
	unsigned char hdr_len;
	unsigned char flags;
	unsigned short window_size;
	unsigned short checksum;
	unsigned short urg_ptr;
	unsigned char data[2048];
	unsigned short len;
};

struct MyTCB
{
	STATE current_state;
	unsigned int local_ip;
	unsigned short local_port;
	unsigned int remote_ip;
	unsigned short remote_port;
	unsigned int seq;
	unsigned int ack;
	unsigned char flags;
	int iotype;
	int is_used;
	int data_ack;
	unsigned char data[2048];
	unsigned short data_len;
};

struct MyTCB gTCB[MAX_TCP_CONNECTIONS]; // TCP connection pool
int initialized = NOT_READY;

// translate address from Network Byte Order (BE) to 
// Host Byte Order (LE) in Tcp segment
int convert_tcp_hdr_ntoh(struct MyTcpSeg* pTcpSeg) {
	if (pTcpSeg == NULL)
		return -1;
	pTcpSeg->src_port = ntohs(pTcpSeg->src_port);
	pTcpSeg->dst_port = ntohs(pTcpSeg->dst_port);
	pTcpSeg->seq_num = ntohl(pTcpSeg->seq_num);
	pTcpSeg->ack_num = ntohl(pTcpSeg->ack_num);
	// knock knock, something is missed here
	pTcpSeg->window_size = ntohs(pTcpSeg->window_size);
	pTcpSeg->checksum = ntohs(pTcpSeg->checksum);
	pTcpSeg->urg_ptr = ntohs(pTcpSeg->urg_ptr);
	return 0;
}

// translate address from Host Byte Order (LE) to 
// Network Byte Order (BE) in Tcp segment
int convert_tcp_hdr_hton(struct MyTcpSeg* pTcpSeg) {
	if( pTcpSeg == NULL )
		return -1;
	pTcpSeg->src_port = htons(pTcpSeg->src_port);
	pTcpSeg->dst_port = htons(pTcpSeg->dst_port);
	pTcpSeg->seq_num = htonl(pTcpSeg->seq_num);
	pTcpSeg->ack_num = htonl(pTcpSeg->ack_num);
	pTcpSeg->window_size = htons(pTcpSeg->window_size);
	pTcpSeg->checksum = htons(pTcpSeg->checksum);
	pTcpSeg->urg_ptr = htons(pTcpSeg->urg_ptr);
	return 0;
}

// called by tcp_kick
// calculate checksum for tcp segment
unsigned short tcp_calc_checksum(struct MyTCB* pTcb, 
								 struct MyTcpSeg* pTcpSeg) {
  int i = 0;
  int len = 0;
  unsigned int sum = 0;
  unsigned short* p = (unsigned short*)pTcpSeg;

  if( pTcb == NULL || pTcpSeg == NULL )
	return 0;

  for( i=0; i<10; i++)
	sum += p[i];

  sum = sum - p[8] - p[6] + ntohs(p[6]);

  // if segment has data
  if ((len = pTcpSeg->len) > 20) {
	if (len%2 == 1) {
	  pTcpSeg->data[len - 20] = 0;
	  len++;
	}

	for (i = 10; i < len/2; i++)
	  sum += ntohs(p[i]);
  }

  //should add ip for checksum
  sum = sum + (unsigned short)(pTcb->local_ip>>16)
			+ (unsigned short)(pTcb->local_ip&0xffff)
			+ (unsigned short)(pTcb->remote_ip>>16)
			+ (unsigned short)(pTcb->remote_ip&0xffff);
  sum = sum + 6 + pTcpSeg->len;
  sum = ( sum & 0xFFFF ) + ( sum >> 16 );
  sum = ( sum & 0xFFFF ) + ( sum >> 16 );
  return (unsigned short)(~sum);
}

// get corresponding socket on the basis of local_port and remote_port
int get_socket(unsigned short local_port, unsigned short remote_port) {
  int i = 1;
  int sockfd = -1;

  for (i = 1; i < MAX_TCP_CONNECTIONS; i++) {
	if (gTCB[i].is_used == USED &&
		gTCB[i].local_port == local_port &&
		gTCB[i].remote_port == remote_port) 
			sockfd = i;
			break;
  }
  return sockfd;
}

// called by tcp_socket() to initialize the corresponding tcp descriptor
int tcp_init(int sockfd) {
  if (gTCB[sockfd].is_used == USED)
	return -1;

  gTCB[sockfd].current_state = CLOSED;
  gTCB[sockfd].local_ip = getIpv4Address();
  gTCB[sockfd].local_port = gLocalPort + sockfd - 1;
  gTCB[sockfd].seq = gSeqNum;
  gTCB[sockfd].ack = gAckNum;
  gTCB[sockfd].is_used = USED;
  gTCB[sockfd].data_ack = DATA_ACKED;
  return 0;
}

// constuct segment for tcp before sent out
int tcp_construct_segment(struct MyTcpSeg* pTcpSeg, struct MyTCB* pTcb, 
						  unsigned short datalen, unsigned char* pData) {	
  pTcpSeg->src_port = pTcb->local_port;
  pTcpSeg->dst_port = pTcb->remote_port;
  pTcpSeg->seq_num = pTcb->seq;
  pTcpSeg->ack_num = pTcb->ack;
  pTcpSeg->hdr_len = (unsigned char)(0x50);
  pTcpSeg->flags = pTcb->flags; 
  pTcpSeg->window_size = 1024;
  pTcpSeg->urg_ptr = 0;

  if( datalen > 0 && pData != NULL )
	memcpy(pTcpSeg->data, pData, datalen);

  pTcpSeg->len = 20 + datalen;
  return 0;
}

// called by tcp_output. tcp_syn_sent
// just kick out the tcp segment to IP layer
// and modify sequence number in TCP connection
// according to specified flag
int tcp_kick(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg) {
	pTcpSeg->checksum = tcp_calc_checksum(pTcb, pTcpSeg); 
	convert_tcp_hdr_hton(pTcpSeg);	
	tcp_sendIpPkt((unsigned char*)pTcpSeg, pTcpSeg->len, 
				   pTcb->local_ip, pTcb->remote_ip, 255);

	// flag field in tcp segment
	if( (pTcb->flags & 0x0f) == 0x00 ) // normal exchange
		pTcb->seq += pTcpSeg->len - 20;
	else if( (pTcb->flags & 0x0f) == 0x02 ) // SYN
		pTcb->seq++;
	else if( (pTcb->flags & 0x0f) == 0x01 ) // FIN
		pTcb->seq++;
	else if( (pTcb->flags & 0x3f) == 0x10 ) // ACK
	{
	}
	return 0;
}

// called by tcp_switch, change state from CLOSED to SYN_SENT
int tcp_closed(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg) {
  
  if (pTcb == NULL || pTcpSeg == NULL)
	return -1;

  if (pTcb->iotype != OUTPUT) {
		//to do: discard packet
		return -1;
  }

  pTcb->current_state = SYN_SENT; // change state
  pTcb->seq = pTcpSeg->seq_num ;

  tcp_kick( pTcb, pTcpSeg );
  return 0;
}

// called by tcp_switch, change state from SYN_SENT to ESTABLISHED
int tcp_syn_sent(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg) {
  struct MyTcpSeg my_seg;
  if (pTcb == NULL || pTcpSeg == NULL)
	return -1;

  if (pTcb->iotype != INPUT)
	return -1;

  // verify server's tcp segment's flag
  if ((pTcpSeg->flags & 0x3f) != 0x12) {
	//to do: discard packet
	return -1;
  }

  pTcb->ack = pTcpSeg->seq_num + 1;
  pTcb->flags = 0x10;

  // further construct syn segment
  tcp_construct_segment(&my_seg, pTcb, 0, NULL );
  tcp_kick(pTcb, &my_seg);

  pTcb->current_state = ESTABLISHED; // finish three-way handshake
  return 0;
}

// handle segment during the established 
int tcp_established(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg) {
  struct MyTcpSeg my_seg;

  if( pTcb == NULL || pTcpSeg == NULL )
	return -1;

  if (pTcb->iotype == INPUT) { // receive
	if (pTcpSeg->seq_num != pTcb->ack) { // sequence number dismatch ack number
	  tcp_DiscardPkt((char*)pTcpSeg, TCP_TEST_SEQNO_ERROR);
	  //to do: discard packet
	  return -1;
	}

	// confirmation of a successfully received segment
	if ((pTcpSeg->flags & 0x3f) == 0x10) { 
	  memcpy(pTcb->data, pTcpSeg->data, pTcpSeg->len - 20);
	  pTcb->data_len = pTcpSeg->len - 20;
	  if (pTcb->data_len == 0)	{ // no data 
	  
	  }	else { // prepare for response, subpath of tcp_recv()
		pTcb->ack += pTcb->data_len;
		pTcb->flags = 0x10; // confirm receiving a tcp segment
		tcp_construct_segment(&my_seg, pTcb, 0, NULL);
		tcp_kick(pTcb, &my_seg);	  
	  }
	}
  }	else {
	if ((pTcpSeg->flags & 0x0F) == 0x01)  // FIN = 1, subpath of tcp_close()
	  pTcb->current_state = FIN_WAIT1;
	// normal send, subpath of tcp_send()
	// tcp segment has already been constructed through call stack
	tcp_kick(pTcb, pTcpSeg);
  }
  return 0;
}

// called in the first tcp_input() of tcp_close()
// Now, the state has already been FIN_WAIT1
// expected to change state to FIN_WAIT2
int tcp_finwait_1(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg) {
  if (pTcb == NULL || pTcpSeg == NULL)
	return -1;

  // shall send FIN to server
  if (pTcb->iotype != INPUT)
	return -1;

  if (pTcpSeg->seq_num != pTcb->ack) { // sequence number dismatch ack number
		tcp_DiscardPkt((char*)pTcpSeg, TCP_TEST_SEQNO_ERROR);
		return -1;
  }

  // sequence number match ack number, forward state
  if( (pTcpSeg->flags & 0x3f) == 0x10 && pTcpSeg->ack_num == pTcb->seq )
	pTcb->current_state = FIN_WAIT2;

  return 0;
}

// called through the second tcp_output() of tcp_close()
// Now, the state has already been FIN_WAIT2
// expected to forward state to TIME_WAIT
int tcp_finwait_2(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg) {
  struct MyTcpSeg my_seg;
  if (pTcb == NULL || pTcpSeg == NULL)
	return -1;

  if (pTcb->iotype != INPUT)
	return -1;

  // FIN is right 
  if (pTcpSeg->seq_num != pTcb->ack) {
	tcp_DiscardPkt((char*)pTcpSeg, TCP_TEST_SEQNO_ERROR);
	return -1;
  }
  // ACK again
  if ((pTcpSeg->flags & 0x0f) == 0x01) {
	pTcb->ack++;
	pTcb->flags = 0x10; // confirm a successfully received segment

	tcp_construct_segment( &my_seg, pTcb, 0, NULL );
	tcp_kick( pTcb, &my_seg );
	pTcb->current_state = CLOSED;
  }	else {
		//to do
  }
	return 0;
}

int tcp_time_wait(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg) {
	pTcb->current_state = CLOSED;
	//to do
	return 0;
}

// called by tcp_input
// check the checkcum in tcp segment
int tcp_check(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg) {
  int i = 0;
  int len = 0;
  unsigned int sum = 0;
  unsigned short* p = (unsigned short*)pTcpSeg;
  unsigned short *pIp;
  unsigned int myip1 = pTcb->local_ip;
  unsigned int myip2 = pTcb->remote_ip;
  if (pTcb == NULL || pTcpSeg == NULL)
	return -1;
  for( i=0; i<10; i++)
	sum = sum + p[i];
  // why translate again, see convert_tcp_hdr_ntoh()
  sum = sum - p[6] + ntohs(p[6]);
  // if this segment has data
  if ((len = pTcpSeg->len) > 20) {
	if (len % 2 == 1) {
	  pTcpSeg->data[len - 20] = 0;
	  len++;
	}

	for (i = 10; i < len/2; i++)
	  sum += ntohs(p[i]);
  }
  // should add ip for tcp checksum
  sum = sum + (unsigned short)(myip1>>16)
			+ (unsigned short)(myip1&0xffff)
			+ (unsigned short)(myip2>>16)
			+ (unsigned short)(myip2&0xffff);
  sum = sum + 6 + pTcpSeg->len;

  sum = ( sum & 0xFFFF ) + ( sum >> 16 );
  sum = ( sum & 0xFFFF ) + ( sum >> 16 );

  if ((unsigned short)(~sum) != 0) {
	// TODO:
	printf("check sum error!\n");
	return -1;
	//return 0;
  } else {
	return 0;
  }
}

// called by tcp_output. tcp_input
// state machine
int tcp_switch(struct MyTCB* pTcb, struct MyTcpSeg* pTcpSeg) {
  int ret = 0;

  switch (pTcb->current_state) {
	case CLOSED:
	  ret = tcp_closed(pTcb, pTcpSeg); // change to SYN_SENT
	  break;
	case SYN_SENT:
	  ret = tcp_syn_sent(pTcb, pTcpSeg); // change to ESTABLISHED
	  break;
	case ESTABLISHED:
	  ret = tcp_established(pTcb, pTcpSeg); 
	  break;
	case FIN_WAIT1:
	  ret = tcp_finwait_1(pTcb, pTcpSeg); // change to FIN_WAIT2
	  break;
	case FIN_WAIT2:
	  ret = tcp_finwait_2(pTcb, pTcpSeg); // change to TIME_WAIT
	  break;
	case TIME_WAIT:
	  ret = tcp_time_wait(pTcb, pTcpSeg);
	  break;
	default:
	  ret = -1;
	  break;
  }
  return ret;
}

// called by tcp_connect
// swallow tcp segment
int tcp_input(char* pBuffer, unsigned short len, 
			  unsigned int srcAddr, unsigned int dstAddr) {
  struct MyTcpSeg tcp_seg;
  int sockfd = -1;

  // header of tcp segment is 20 Bytes
  if( len < 20 )
	return -1;

  memcpy(&tcp_seg, pBuffer, len);
  tcp_seg.len = len;
  convert_tcp_hdr_ntoh(&tcp_seg);

  // get socket according to port number 
  sockfd = get_socket(tcp_seg.dst_port, tcp_seg.src_port);

  // further check if the sockfd is right and corresponding ip is right
  // just like the counterpart in tcp_output
  if (sockfd == -1 || 
	  gTCB[sockfd].local_ip != ntohl(dstAddr) || 
	  gTCB[sockfd].remote_ip != ntohl(srcAddr)) {
	printf("sock error in tcp_input()\n");
	return -1;
  }

  // check checksum
  if (tcp_check(&gTCB[sockfd], &tcp_seg) != 0)
		return -1;

  gTCB[sockfd].iotype = INPUT; // could be verify in tcp_syn_sent()
  memcpy(gTCB[sockfd].data, tcp_seg.data, len - 20);
  gTCB[sockfd].data_len = len - 20; // minus header's length

  tcp_switch(&gTCB[sockfd], &tcp_seg);
  return 0;
}

// construct tcp segment and send out
void tcp_output(char* pData, unsigned short len, unsigned char flag, 
				unsigned short srcPort, unsigned short dstPort, 
				unsigned int srcAddr, unsigned int dstAddr) {
  struct MyTcpSeg my_seg;
  sockfd = get_socket(srcPort, dstPort);

  // check if the targeted socket has been found and
  // verify that src addr and des addr are right
  if (sockfd == -1 || 
	  gTCB[sockfd].local_ip != srcAddr || 
	  gTCB[sockfd].remote_ip != dstAddr)
	return;

  // flag is used in tcp_kick function
  gTCB[sockfd].flags = flag;
  // further construct tcp segment
  tcp_construct_segment(&my_seg, &gTCB[sockfd], len, (unsigned char *)pData);
  gTCB[sockfd].iotype = OUTPUT;
  tcp_switch(&gTCB[sockfd], &my_seg); 
}

// allocate a socket descriptor
int tcp_socket(int domain, int type, int protocol) {
  int i = 1;
  int sockfd = -1;

  // The protocol family which indicate the network medium in socket communication
  // should be AF_INET (Internet network)
  // The socket typoe should be SOCK_STREAM (implemented by TCP/IP in AF_INET)
  // another communication mechanism in AF_INET shall be SOCK_DGRAM implemented 
  // by UDP/IP
  // The protocol should be IPPTOTO_TCP
  if (domain != AF_INET || type != SOCK_STREAM || protocol != IPPROTO_TCP)
		return -1;

  //allocate one free socket descriptor
  for (i = 1; i < MAX_TCP_CONNECTIONS; i++ ) {
	if (gTCB[i].is_used == NOT_USED) {
	  sockfd = i;
	  if (tcp_init(sockfd) == -1) //initialize sockfd
		return -1;
	  break;
	}
  }
  initialized = READY;
  return sockfd;
}

// build tcp connection, indicating the sockfd, server's addr and addrlen (deprecated)
int tcp_connect(int sockfd, struct sockaddr_in* addr, int addrlen) {
	char buffer[2048];
	int len;

	// translate address from Network Byte Order (big endian) to 
	// Host Byte Order (little endian) 
	// ntohl(long), ntohs(short)
	gTCB[sockfd].remote_ip = ntohl(addr->sin_addr.s_addr); 
	gTCB[sockfd].remote_port = ntohs(addr->sin_port);

	tcp_output(NULL, 0, 0x02, 
			   gTCB[sockfd].local_port, gTCB[sockfd].remote_port, 
			   gTCB[sockfd].local_ip, gTCB[sockfd].remote_ip);

	// actively receive IP Packet from IP layer, waiting for 10 seconds
	// buffer is tcp segment
	len = waitIpPacket(buffer, 10);

	// header of tcp segment is 20 Bytes
	if (len < 20)
		return -1;

	if (tcp_input(buffer, len, 
		  		  htonl(gTCB[sockfd].remote_ip), htonl(gTCB[sockfd].local_ip)) != 0)
		return 1;
	else
		return 0;


// send to server after establishing tcp connection
int tcp_send(int sockfd, const unsigned char* pData, 
			 unsigned short datalen, int flags) {
  char buffer[2048];
  int len;

  if (gTCB[sockfd].current_state != ESTABLISHED)
	return -1;

  tcp_output((char *)pData, datalen, flags, 
	  		  gTCB[sockfd].local_port, gTCB[sockfd].remote_port, 
			  gTCB[sockfd].local_ip, gTCB[sockfd].remote_ip);

  len = waitIpPacket(buffer, 10);

  // header of tcp segment is at least 20 Bytes
  if( len < 20 )
	return -1;

  tcp_input(buffer, len, 
	  		htonl(gTCB[sockfd].remote_ip), 
			htonl(gTCB[sockfd].local_ip));
  return 0;
}

// passively receive from server
int tcp_recv(int sockfd, unsigned char* pData, unsigned short datalen, int flags) {
  char buffer[2048];
  int len;

  if ((len = waitIpPacket(buffer, 10)) < 20)
	return -1;

  tcp_input(buffer, len,  
	  		htonl(gTCB[sockfd].remote_ip),
			htonl(gTCB[sockfd].local_ip));

  memcpy(pData, gTCB[sockfd].data, gTCB[sockfd].data_len);

  return gTCB[sockfd].data_len;
}

// close tcp connection
int tcp_close(int sockfd) {
  char buffer[2048];
  int len;
  
  // change state to FIN WAIT 1
  tcp_output(NULL, 0, 0x11, 
	  		 gTCB[sockfd].local_port, gTCB[sockfd].remote_port, 
			 gTCB[sockfd].local_ip, gTCB[sockfd].remote_ip);

  // length of header of tcp segment is at least 20
  if( (len = waitIpPacket(buffer, 10)) < 20 )
	return -1;

  // change state to FIN WAIT 2, half closed
  tcp_input(buffer, len, 
	  		htonl(gTCB[sockfd].remote_ip), 
			htonl(gTCB[sockfd].local_ip));

  if( (len = waitIpPacket(buffer, 10)) < 20 )	
	return -1;

  // change state to TIME WAIT
  tcp_input(buffer, len, 
	  		htonl(gTCB[sockfd].remote_ip), 
			htonl(gTCB[sockfd].local_ip));

  // close done
  gTCB[sockfd].is_used = NOT_USED;
  return 0;
}
