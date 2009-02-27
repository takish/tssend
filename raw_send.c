/*
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>

#include "raw_send.h"

int
raw_udpip_init()
{
    int sockfd;
    int on = 1;

    if ((sockfd = socket(PF_INET, SOCK_RAW, IPPROTO_RAW)) == -1) {
	perror("socket");
	exit(1);
    }

    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	exit(1);
    }

    return sockfd;
}


/*
+------------------------------------------------------------------+
|IP Header fields modified on sending when IP_HDRINCL is specified |
+------------------------------------------------------------------+
|  Sending fragments with IP_HDRINCL is not supported currently.   |
+--------------------------+---------------------------------------+
|IP Checksum               |Always filled in.                      |
+--------------------------+---------------------------------------+
|Source Address            |Filled in when zero.                   |
+--------------------------+---------------------------------------+
|Packet Id                 |Filled in when passed as 0.            |
+--------------------------+---------------------------------------+
|Total Length              |Always filled in.                      |
+--------------------------+---------------------------------------+
 */

void
fill_udpip_hdr(char *packet,
	       char *src_host,
	       char *dst_host,
	       char *dest_port,
		   int p_len)
{
    struct iphdr *iph;
    struct udphdr *udph;
    unsigned long saddr, daddr;
	int dport;

	dport = atoi(dest_port);

    saddr = translate_hostname(src_host);
    daddr = translate_hostname(dst_host);

    iph = (struct iphdr *)packet;
    udph = (struct udphdr *)(packet + sizeof(struct iphdr));

    iph->version = 4;
    iph->ihl = 5;
    iph->tos = 0;
    iph->tot_len = 0;
    iph->id = 0;
    iph->frag_off = 0;
    iph->ttl = 16;
    iph->protocol = IPPROTO_UDP;
    iph->check = 0;
    iph->saddr = saddr;
    iph->daddr = daddr;

    udph->source = htons(53);  /* dummy */
    udph->dest = htons(dport);
    udph->len = htons(p_len + sizeof(struct udphdr));
    udph->check = 0;

    return;
}

unsigned int
parse_cmdline(int argc,
	      char *argv[],
	      char *src_host,
	      char *dst_host,
	      int *dport)
{
    int c;
    unsigned int argsfound = 0;
    extern char *optarg;
    extern int optind;
    
    while ((c = getopt(argc, argv, "s:d:p:")) != -1) {
	switch ((char)c) {
	case 's':
	    argsfound |= SHOSTFOUND;
	    strncpy(src_host, optarg, MAXHOSTNAMELEN);
	    fprintf(stderr, "src host %s\n", src_host);
	    break;
	case 'd':
	    argsfound |= DHOSTFOUND;
	    strncpy(dst_host, optarg, MAXHOSTNAMELEN);
	    fprintf(stderr, "dst host %s\n", dst_host);
	    break;
	case 'p':
	    argsfound |= PORTFOUND;
	    *dport = (int)strtol(optarg, NULL, 10);
	    fprintf(stderr, "dst port = %d\n", *dport);
	    break;
	default:
	    fprintf(stderr, "Unknown option %c\n", c);
	    exit(0);
	    break;
	}
    }
    
    return argsfound;
}

unsigned long
translate_hostname(char *hostname)
{
    unsigned long addr;
    struct hostent *serv_host;
    
    if (isdigit(hostname[0])) {
	addr = inet_addr(hostname);
    } else {
	serv_host = gethostbyname(hostname);
	bcopy(serv_host->h_addr, (char *)&addr, sizeof(addr));
    }
    
    return addr;
}
