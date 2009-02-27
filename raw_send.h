/*
 *
 */

#ifndef __RAW_SEND_H__
#define __RAW_SEND_H__

#define SHOSTFOUND  0x01
#define DHOSTFOUND  0x02
#define PORTFOUND   0x04
#define ARGSMIN     (PORTFOUND | SHOSTFOUND | DHOSTFOUND)

#define SAMPLE_MESSAGE "This is raw ip send test.\n"

int
raw_udpip_init();

void
fill_udpip_hdr(char *packet,
	       char *src_host,
	       char *dst_host,
	       char *dport,
	       int p_len);

unsigned int
parse_cmdline(int argc, char *argv[], 
	      char *src_host, 
	      char *dst_host,
	      int  *dport);

unsigned long
translate_hostname(char *hostname);

#endif
