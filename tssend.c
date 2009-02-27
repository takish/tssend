#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <signal.h>

//#define RTC

#define UNSPEC

#include "rtc.h"
#include "raw_send.h"

//#include "sock.h"
#if 0
#include<linux/spinlock.h>
#include<linux/mc146818rtc.h>
#endif

#define DEFAULT_PL 1400
#define MAX_PL 2000
#define RTC_DEFAULTS_HZ 8192
#define TIMEOUT_SEC 1
#define TIMEOUT_USEC 0
#define DEFAULT_PKT_NUM 1024
#define SECOND 1000000 //usec

// MODE
#define SERVER 0
#define CLIENT 1

#define RECV_NUM 100
#define INTVAL 30

#define SEQ_MAX 65000
#define SOCK_BUFLEN 1024*200

int send_sock_create(const char *host, const char *serv, 
		     void ** saptr, socklen_t *lenp);
int recv_sock_create(const char *host, const char *serv, 
		     socklen_t *addrlenp);
void client(int hcnt, char *hname[], char *port, int vseed, int pkt_num, char *send_addr, int ttl);
void server(char *port, char *m_addr);

struct packet{
	unsigned char pt;
	unsigned long long time;
	unsigned long seq;
	char payload[MAX_PL];
};

struct packet pkt;

struct itimerval timer;

// external value
long pkt_count = 0; 
int mode = CLIENT;
int fract_lost;
int fract_cnt;
int burst_lost;
int burst_cnt;
static int cumulate_lost = 0;
static int cumulate_cnt = 0;
//int payload;
int change;
int p_len = DEFAULT_PL;
long jitter = 0;

void intr_handler(int n){
    cumulate_lost = 0;
    cumulate_cnt = 0;
    jitter = 0;
}

void update_handler(int n){
}

void
display_log(int sig){
	time_t	t;
	char	timep[30];
	static int cnt;
	static int int_lost = 0;
	static int int_cnt = 0;
	double loss_rate = 0.0;
	double cumu_loss_rate = 0.0;
	static int start = 0;

	time(&t);
	strcpy(timep, ctime(&t));
	timep[strlen(ctime(&t)) - 6] = '\0';

	switch(mode)
	{
	case SERVER:
		if(cumulate_lost != 0){
			cumu_loss_rate = (float)cumulate_lost/(cumulate_lost + cumulate_cnt) * 100;
		}

		if(cnt % INTVAL == 0){
			if(start != 0){
			printf("# \n");
				printf("# Interval Report [%d sec]\n", INTVAL);
				printf("#  Cumulative Lost:%6d Cumulative Packet:%6d  Packet Loss Rate:%2.2f % \n", 
				int_lost, int_cnt, (float)int_lost/(int_lost + int_cnt) * 100);
				printf("# \n");
			}
				start = 1;

			printf("[  time  ]   Fract Lost   Fract Recv    Total Lost   Jitter[ms]  Loss Rate[%]\n");
			printf("----------+-------------+------------+--------------+----------+----------\n");
			int_cnt = 0;
			int_lost = 0;
			cnt = 0;
			cumulate_lost = 0;
			cumulate_cnt = 0;
			loss_rate += cumu_loss_rate;
		} 
			printf("[%s] %4d(%3d,%3d)    %4d(%3d)      %6d      %2.3f      %.2f\n",
		timep + 11, fract_lost, burst_lost, burst_cnt, fract_cnt, change, cumulate_lost, (float)jitter/1000 , cumu_loss_rate);
		cumulate_lost += fract_lost;
		cumulate_cnt += fract_cnt;
		int_lost += fract_lost;
		int_cnt += fract_cnt;
		fract_cnt = 0;
		fract_lost = 0;
		burst_lost = 0;
		burst_cnt = 0;
		cnt++;
	break;
	case CLIENT:
		if(cnt % INTVAL == 0){
			printf("[ time  ]     Sending Rate     Fract Send    Payload \n");
			printf("----------+------------------+----------+---------------\n");
			cnt = 0;
		}
		printf("[%s]     %.2f (Mbps)        %4d       %4d (byte)\n",
		timep + 11, (double)(pkt_count*p_len*8)/1024/1024, pkt_count, p_len);
		pkt_count = 0;
		cnt++;
	break;
	}
}

void
usage(void)
{
	printf("Usage: tssend [options ... ] dest_addr\n");
	printf("\n");
	printf("options\n");
	printf("\t-s\tRun server mode\n");
	printf("\t-b\tSet bandwidth[Mbps]\n");
	printf("\t-l\tSet payload length (default 1400)\n");
	printf("\t-p\tSet sending port\n");
	printf("\t-P\tSet receive port\n");
	printf("\t-v\tvariable width [Mbps]\n");
	printf("\t-n\tSet packet count per sec\n");
	printf("\t-m\tSet multicast addr for joining\n");
	printf("\t-t\tSet multicast ttl\n");
	printf("\t-a\tAppend dest addr\n");
	printf("\t-r\tFor fake source addr\n");
	exit(-1);
}

int
main(int argc, char *argv[]) {

	char *hname[2];
	char *m_addr = NULL;
	char *port = "9000";
	char *serv_port = "9000";
	int sw;
	int pkt_num = DEFAULT_PKT_NUM;
	int hcnt = 1;
	int vseed = 0;
	int band = 0;
	char *send_addr = "NULL";
	int ttl;

	signal(SIGALRM, display_log);

	timer.it_interval.tv_sec = TIMEOUT_SEC;
	timer.it_interval.tv_usec = TIMEOUT_USEC;
	timer.it_value.tv_sec = TIMEOUT_SEC;
	timer.it_value.tv_usec = TIMEOUT_USEC;

	setitimer(ITIMER_REAL, &timer, NULL);

	while((sw = getopt(argc, argv, "r:sp:l:v:n:a:m:P:t:b:h")) != EOF){
		switch(sw){
			case 's':
				mode = SERVER;
				break;
			case 'b':
				band = atoi(optarg);
				break;
			case 'l':
				p_len = atoi(optarg);
				break;
			case 'p':
				port = optarg;
			break;
			case 'P':
				serv_port = optarg;
			break;
			case 'v':
				vseed = atoi(optarg);
			break;
			case 'n':
				pkt_num = atoi(optarg);
			break;
			case 'm':
				m_addr = optarg;
			break;
			case 't':
				ttl = atoi(optarg);
			break;
			case 'a':
				hname[1] = optarg;
			hcnt++;
			break;
			case 'r':
				send_addr = optarg;
			break;
			case 'h':
			default:
				usage();
			break;
		}
	}

	if(argc < 2){
		usage();
	}

	argc -= optind;
	argv += optind;

#ifdef RTC
    if(geteuid() != 0){
        fprintf(stderr,"#\n# This program requires superuser privilege.\n# You must have \"root\" privilege to execute this program.\n#\n");
        exit(0);
    }
#endif

	if((argc < 1)&&(mode == CLIENT)){
		printf("Please indicate destination address!\n");
		exit(-1);
	}

	hname[0] = argv[0];
//	payload = p_len;

	setpriority(PRIO_PROCESS, 0, -20);

	signal(SIGINT, intr_handler);
	signal(SIGHUP, update_handler);
 
	if(mode == CLIENT){     
			if(band > 0){
					band = band * 1024 * 1024 / 8;

					if(p_len != DEFAULT_PL){
							pkt_num = band / p_len;
					}
					else if(pkt_num != DEFAULT_PKT_NUM){
							p_len = band / pkt_num;
					} else {
						pkt_num = band / DEFAULT_PL;
					}
//					printf("%d %d\n", p_len, pkt_num);
			}
			client(hcnt,hname,port,vseed,pkt_num,send_addr,ttl);
	}

	if(mode == SERVER){
			server(serv_port, m_addr);
	}
}

void show_client_init(char *port, int pkt_num, int p_len){
    printf("#\n");
	printf("# [ Client Mode ]\n");
	printf("# Send port : %s\n", port);
	printf("# Packet/sec : %d\n", pkt_num);
	printf("# Payload size : %d byte\n",p_len);
    printf("#\n");
}

void client(int hcnt, char *hname[], char *port, int vseed, int pkt_num, char *send_addr, int ttl){
    unsigned long long base = 0, now = 0, bef = 0;
	static int new_set = 0;
	int i, j;
	int ssock[2], gai_error;
	struct sockaddr *your_addr[2];
	socklen_t your_adlen[2];
	int sock_buflen;
	struct timeval gtod;
	int vpkt = 0;
	int intval = 1;
	int seq;
	int rtc;
	char packet[2][1500];
	struct sockaddr_in dest_sin;
	struct timeval tv;
	unsigned long lTTL = 1;

#ifdef RTC
	rtc = open_rtc(RTC_DEFAULTS_HZ);
#endif
    show_client_init(port, pkt_num, p_len);

	memset(&pkt, 0, sizeof(struct packet));

	sock_buflen = SOCK_BUFLEN;

	for(i = 0; i < hcnt; i++){
		if(send_addr != "NULL"){
			ssock[i] = raw_udpip_init();
			fill_udpip_hdr(packet[i], send_addr, hname[i], port, p_len); 
		} else {
			ssock[i] = send_sock_create(hname[i], port, (void **)&your_addr[i], &your_adlen[i]);
			setsockopt(ssock[i], SOL_SOCKET, SO_SNDBUF, &sock_buflen, sizeof(sock_buflen));
			if(ttl>lTTL){
				lTTL = (unsigned long)ttl;
				if (setsockopt(ssock[i], IPPROTO_IP, IP_MULTICAST_TTL, (void *)&lTTL, sizeof(lTTL)) < 0){
      				perror("Set TTL failed");
		  		}
			}
//		if((connect(ssock[i], (struct sockaddr *)&your_addr[i], your_adlen[i])) < 0) 
//				perror("connect failed\n");
		}
	}


	gettimeofday(&gtod, NULL);
	base = gtod.tv_sec*1E6 + gtod.tv_usec;
	now = base;
	bef = base;

	while(1) {
#ifdef RTC
		if(vseed){
			vpkt = random() % vseed;
		}

		for(i = 1; i <= 12; i++){
			if((pkt_num + vpkt) <= (2 << i)){
					intval = RTC_DEFAULTS_HZ/(2 << i);
					break;
			}
		}

		for(j = 0; j < RTC_DEFAULTS_HZ; j++){
			if((j % intval) == 0){
#endif
/*				for(i = 0;i < p_len; i++){
					pkt.payload[i] = random() & 0xff;
				}
*/
				bef = now;
				pkt.time = now - base;
				pkt.seq = seq;

				if(seq < 100 && new_set ==0){
					pkt.pt = 33;
					new_set = 1;
				} else {
					pkt.pt = 44;
				}

				if(pkt_count < (pkt_num + vpkt)){
					if(pkt_count == 0){
						pkt.pt = 32;
					}

					for(i = 0;i < hcnt; i++){
						if(send_addr != "NULL"){
							memcpy(packet[i] + sizeof(struct udphdr) + sizeof(struct iphdr), &pkt, p_len);
							dest_sin.sin_family = AF_INET;
							dest_sin.sin_addr.s_addr = inet_addr(hname[i]);
							if(sendto(ssock[i], &packet[i], p_len + sizeof(struct udphdr) + sizeof(struct iphdr), 0, (struct sockaddr *)&dest_sin, sizeof(dest_sin)) < 0) {
							printf("sendto failed\n");
							exit(1);
						}
						} else {
						if(sendto(ssock[i], &pkt, p_len, 0, your_addr[i], your_adlen[i]) < 0) {
							printf("unspec sendto failed\n");
							exit(1);
						}
						}
/*						if(send(ssock[i], &pkt, p_len, 0) < 0){
							printf("send failed\n");
							exit(1);
						}
						*/
					}

//printf("%d %d\n",seq,intval);
					seq++;
					pkt_count++;

					if(seq == SEQ_MAX){
						seq = 0;
					}
				}
#ifdef RTC
			}
				wait_rtc(rtc);
		}
#endif
	}
	for(i = 0; i < hcnt; i++){
		close(ssock[i]);
	}
}

void show_server_init(char *port){
    printf("#\n");
    printf("# [ Server Mode ]\n");
	printf("# Receive Port : %s\n", port);
    printf("#\n");
}

void server(char *port, char *m_addr){
    long long base = 0, now = 0, int_sec = 0, fract_jit = 0;
	int last_seq;
	int fract_burst = 0;
	static int intval_cnt = 0;
	int last_buf;
	int rsock;
	socklen_t your_adlen;
	int sock_buflen;
	struct ip_mreq multicastRequest;
	int i;
	struct addrinfo hints;
	int length = sizeof(hints);
	struct timeval gtod;
	int buffer[RECV_NUM];
	int rc;

    show_server_init(port);

	sock_buflen = SOCK_BUFLEN;

#ifdef UNSPEC
    rsock = recv_sock_create(NULL, port, &your_adlen);
#else
	rsock = ServerSocketU(port,-1);
#endif
    setsockopt(rsock, SOL_SOCKET, SO_RCVBUF, &sock_buflen, sizeof(sock_buflen));

    if(m_addr){
        /* indicate multicast group */
          multicastRequest.imr_multiaddr.s_addr = inet_addr(m_addr);
          /* Accept multicast from interface */
          multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
          /* Join the multicast group */
          if (setsockopt(rsock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&multicastRequest, sizeof(multicastRequest)) < 0){
      		perror("JOIN failed");
          }

      }

	if(recvfrom(rsock, &pkt, sizeof(pkt),0,
			(struct sockaddr *)&hints, &length) < 0) exit(1);

//	if((connect(rsock, (struct sockaddr *)&hints, length)) < 0) exit(1);

    while(1){

		rc = recv(rsock, &pkt, sizeof(pkt), 0);

		if(rc < 0)
				perror("recv failed");

		fract_cnt++;

		if(pkt.pt == 32){
			gettimeofday(&gtod, NULL);
			now = gtod.tv_sec*1E6 + gtod.tv_usec;
			int_sec = now - base;
				if(int_sec > SECOND){
					fract_jit = int_sec - SECOND;
				} else {
					fract_jit = SECOND - int_sec;
				}
				if(fract_jit < (SECOND/2)) {
					jitter = (jitter * 7 / 8) + fract_jit / 8;
				}
			base = now;
		}

/*		if(pkt.seq == 0){
			base = gtod.tv_sec*1E6 + gtod.tv_usec - pkt.time;
		}
		now = gtod.tv_sec*1E6 + gtod.tv_usec;
*/

		if(pkt.pt == 33){
			fract_lost = 0;
			fract_cnt = 0;
			last_seq = pkt.seq - 1;
		}
//      printf("%lld %lld %ld\n",pkt.time, now - base, pkt.seq);

//	      	buffer[pkt.seq % RECV_NUM] = 0;
		buffer[pkt.seq % RECV_NUM] = intval_cnt;
		intval_cnt++;

		if(pkt.seq % RECV_NUM == 0){
			for(i = 0; i < RECV_NUM; i++){
				if(buffer[i] < 0){
//							printf("%d\n", pkt.seq);
					fract_lost++;
					fract_burst++;
				} else {
					if(fract_burst > burst_lost){
						burst_lost = fract_burst;
					}

					if(fract_burst > 1){
						burst_cnt++;
					}

					fract_burst = 0;

					if((last_buf + 1) == buffer[i]){
						change++;
					}
						last_buf = buffer[i];
					}
				}

				for(i = 0; i < RECV_NUM; i++){
					buffer[i] = -1;
				}

				intval_cnt = 0;
				change = 0;
			}

/*      if((pkt.seq - 1) != last_seq){
			  fract_lost++;
	  } else {
			  fract_cnt++;
	  }
	  */
			last_seq = pkt.seq;
		}
}

int send_sock_create(const char *host, const char *serv, void ** saptr, 
socklen_t *lenp)
{
  int sofd, n;
  struct addrinfo hints, *res, *ressave;
  
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  
  if((n = getaddrinfo(host, serv, &hints, &res)) != 0)
    {
      fprintf(stderr,"error_getaddrinfo %s %s %s\n", host, serv, 
gai_strerror(n));
      exit(1);
    }
  ressave = res;
  
  do {
    sofd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sofd >= 0)
      {
		break;
      }
	if(connect(sofd, res->ai_addr, res->ai_addrlen) < 0){
		perror("connect failed\n");
		close(sofd);
		sofd = -1;
		continue;
	}

  }while((res = res->ai_next) != NULL);
  
  if(res == NULL)
    {
      fprintf(stderr,"send sock error for %s %s\n", host, serv);
    }
  *saptr = (struct ai_addrlen *)malloc(res->ai_addrlen);
  memcpy(*saptr, res->ai_addr, res->ai_addrlen);
  *lenp = res->ai_addrlen;
  
  freeaddrinfo(ressave);
  return(sofd);
}

int recv_sock_create(const char *host, const char *serv, socklen_t 
*addrlenp)
{
  int sofd, n;
  struct addrinfo hints, *res, *ressave;
  
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  
  if((n = getaddrinfo(host, serv, &hints, &res)) != 0)
    {
      fprintf(stderr,"error_getaddrinfo %s %s %s\n", host, serv, 
gai_strerror(n));
      exit(1);
    }
  ressave = res;

  do {
    sofd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sofd < 0)
      {
continue;
      }
    if(bind(sofd, res->ai_addr, res->ai_addrlen) == 0)
      {
break;
      }
    
    close(sofd);
  }while((res = res->ai_next) != NULL);
  
  if(res == NULL)
    {
      fprintf(stderr,"send sock error for %s %s\n", host, serv);
    }
  
  if(addrlenp)
    {
      *addrlenp = res->ai_addrlen;
    }
  
  freeaddrinfo(ressave);
  return(sofd);
}
