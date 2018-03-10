/*
 * :ts=4
 *
 * TFTP client program for the Amiga, using only the SANA-II network
 * device driver API, and no TCP/IP stack
 *
 * The "trivial file transfer protocol" is anything but trivial
 * to implement...
 *
 * Copyright © 2016 by Olaf Barthel <obarthel at gmx dot net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. MY CAPS LOCK KEY SEEMS TO BE STUCK.
 */

#ifndef _NETWORK_IP_UDP_H
#define _NETWORK_IP_UDP_H

/****************************************************************************/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif /* EXEC_TYPES_H */

/****************************************************************************/

/*
 * UDP protocol header.
 * Per RFC 768, September, 1981.
 */
struct udphdr
{
	UWORD	uh_sport;	/* source port */
	UWORD	uh_dport;	/* destination port */
	WORD	uh_ulen;	/* udp length */
	UWORD	uh_sum;		/* udp checksum */
};

/****************************************************************************/

/*
 * Definitions for internet protocol version 4.
 * Per RFC 791, September 1981.
 */
#define	IPVERSION	4

#define	IPPROTO_ICMP	1	/* control message protocol */
#define	IPPROTO_UDP		17	/* user datagram protocol */

struct ip
{
	UBYTE	ip_v_hl;		/* ip version and header length */
	UBYTE	ip_tos;			/* type of service */
	WORD	ip_len;			/* total length */
	UWORD	ip_id;			/* identification */
	WORD	ip_off;			/* fragment offset field */
	UBYTE	ip_ttl;			/* time to live */
	UBYTE	ip_pr;			/* protocol */
	UWORD	ip_sum;			/* checksum */
	ULONG	ip_src,ip_dst;	/* source and dest address */
};

/****************************************************************************/

/* Combined IP and UDP headers, suitable for calculating
 * and verifying the UDP datagram checksum.
 */
struct udp_pseudo_header
{
	ULONG	ip_zero1[2];	/* set to zero; this covers ip_v_hl..ip_off */
	UBYTE	ip_zero2;		/* set to zero; this covers ip_ttl */
	UBYTE	ip_pr;			/* protocol */
	UWORD	ip_len;			/* protocol length; this covers ip_sum */
	ULONG	ip_src;			/* source internet address; this covers ip_src */
	ULONG	ip_dst;			/* destination internet address; this covers ip_dst */

	UWORD	uh_sport;		/* source port */
	UWORD	uh_dport;		/* destination port */
	WORD	uh_ulen;		/* udp length */
	UWORD	uh_sum;			/* udp checksum */
};

/****************************************************************************/

/* Basic ICMP header; there may be other data following it.  */
struct icmp_header
{
	UBYTE type;
	UBYTE code;
	UWORD checksum;
};

/* ICMP message for "destination unreachable". */
struct icmp_unreachable_header
{
	struct icmp_header	header;
	ULONG				unused;
	struct ip			ip;
};

/****************************************************************************/

/* Definition of type and code field values for ICMP. We are
 * only interested in the "unreachable" part.
 */
enum icmp_errors_and_codes
{
	icmp_type_unreach=3,				/* destination unreachable */
	icmp_code_unreach_net=0,			/* bad net */
	icmp_code_unreach_host=1,			/* bad host */
	icmp_code_unreach_protocol=2,		/* bad protocol */
	icmp_code_unreach_port=3,			/* bad port */
	icmp_code_unreach_needfrag=4,		/* IP_DF caused drop */
	icmp_code_unreach_srcfail=5,		/* src route failed */
	icmp_code_unreach_net_unknown=6,	/* unknown net */
	icmp_code_unreach_host_unknown=7,	/* unknown host */
	icmp_code_unreach_isolated=8,		/* src host isolated */
	icmp_code_unreach_net_prohib=9,		/* prohibited access */
	icmp_code_unreach_host_prohib=10,	/* ditto */
	icmp_code_unreach_tosnet=11,		/* bad tos for net */
	icmp_code_unreach_toshost=12,		/* bad tos for host */
};

/****************************************************************************/

extern int in_cksum (const void * addr, int len);
extern int inet_aton(const char *cp, unsigned long * addr);
extern LONG send_udp(int client_port_number,int server_port_number,const void * data,int data_length);
extern int verify_udp_datagram_checksum(struct ip * ip);
extern void get_ipv4_address_and_path_from_name(STRPTR name, ULONG * ipv4_address, STRPTR * path_name);

/****************************************************************************/

#endif /* _NETWORK_IP_UDP_H */
