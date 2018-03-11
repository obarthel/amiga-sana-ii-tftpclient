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

#include <string.h>
#include <ctype.h>

/****************************************************************************/

#define __USE_INLINE__
#include <proto/exec.h>
#include <proto/dos.h>

/****************************************************************************/

#include "testing.h"
#include "network-io.h"
#include "network-ip-udp.h"

/****************************************************************************/

#include "macros.h"
#include "assert.h"

/****************************************************************************/

/* in_cksum() was borrowed from the 4.4BSD-Lite2 "ping.c" command.
 *
 * Copyright (c) 1982, 1986, 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * in_cksum --
 *      Checksum routine for Internet Protocol family headers (C Version)
 */
int
in_cksum (const void * addr, int len)
{
	int nleft = len;
	const UWORD *w = addr;
	int sum = 0;
	UWORD answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while(nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if(nleft == 1)
	{
		*(UBYTE *) (&answer) = *(UBYTE *) w;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
	sum += (sum >> 16); /* add carry */
	answer = ~sum; /* truncate to 16 bits */

	return (answer);
}

/****************************************************************************/

/* inet_aton() was borrowed from the 4.4BSD-Lite2 libc code.
 *
 * Copyright (c) 1982, 1986, 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
 * Check whether "cp" is a valid ascii representation
 * of an Internet address and convert to a binary address.
 * Returns 1 if the address is valid, 0 if not.
 * This replaces inet_addr, the return value from which
 * cannot distinguish between failure and a local broadcast address.
 */
int
inet_aton(const char *cp, unsigned long * addr)
{
	unsigned long val = 0;
	int base, n;
	char c;
	unsigned long parts[4];
	unsigned long *pp = parts;

	for (;;)
	{
		/*
		 * Collect number up to ``.''.
		 * Values are specified as for C:
		 * 0x=hex, 0=octal, other=decimal.
		 */
		val = 0;
		base = 10;

		if (*cp == '0')
		{
			if (*++cp == 'x' || *cp == 'X')
				base = 16, cp++;
			else
				base = 8;
		}

		while ((c = *cp) != '\0')
		{
			if (isascii(c) && isdigit(c))
			{
				val = (val * base) + (c - '0');
				cp++;
				continue;
			}

			if (base == 16 && isascii(c) && isxdigit(c))
			{
				val = (val << 4) +  (c + 10 - (islower(c) ? 'a' : 'A'));
				cp++;
				continue;
			}

			break;
		}

		if (*cp == '.')
		{
			/*
			 * Internet format:
			 *	a.b.c.d
			 *	a.b.c	(with c treated as 16-bits)
			 *	a.b	(with b treated as 24 bits)
			 */
			if (pp >= parts + 3 || val > 0xff)
				return (0);

			*pp++ = val, cp++;
		}
		else
		{
			break;
		}
	}

	/*
	 * Check for trailing characters.
	 */
	if (*cp && (!isascii(*cp) || !isspace(*cp)))
		return (0);

	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts + 1;

	switch (n)
	{
		case 1: /* a -- 32 bits */

			break;

		case 2: /* a.b -- 8.24 bits */

			if (val > 0xffffff)
				return (0);

			val |= parts[0] << 24;
			break;

		case 3: /* a.b.c -- 8.8.16 bits */

			if (val > 0xffff)
				return (0);

			val |= (parts[0] << 24) | (parts[1] << 16);
			break;

		case 4: /* a.b.c.d -- 8.8.8.8 bits */

			if (val > 0xff)
				return (0);

			val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
			break;

		default:

			return(0);
	}

	if (addr)
		*addr = val;

	return (1);
}

/****************************************************************************/

/* Fill the write request transmission buffer with an IP datagram, containing
 * a UDP datagram, which in turn contains protocol data for the TFTP server to
 * receive. The UDP datagram will be initialized to use the given source and
 * destination port numbers. We also calculate both the UDP and IP datagram
 * checksums so that transmission errors are less likely to be overlooked.
 *
 * Once the IP and UDP headers have been filled in, and the checksums have
 * been calculated, the entire IP datagram is sent to the TFTP server.
 */
LONG
send_udp(int client_port_number,int server_port_number,const void * data,int data_length)
{
	UBYTE * packet = write_request->nior_Buffer;
	struct udphdr * udp;
	struct ip * ip;
	struct udp_pseudo_header * udp_pseudo_header;
	LONG error;
	int len;

	ENTER();

	ASSERT( write_request->nior_BufferSize > 540 );
	ASSERT( data_length <= write_request->nior_BufferSize );

	/* Set the entire buffer contents to 0 because the
	 * UDP payload length may not be an even number. In
	 * this case a padding byte may have to be added,
	 * and it should be set to 0.
	 */
	memset(packet,0,write_request->nior_BufferSize);

	ip = (struct ip *)packet;
	udp = (struct udphdr *)&ip[1];

	/* Fill in the UDP datagram payload. */
	memmove(&udp[1], data, data_length);

	len = data_length;

	/*
	 * Set up the UDP contents.
	 */

	/* The datagram length must be even. */
	if ((len % 2) != 0)
		len++;

	len += sizeof(*udp);

	udp->uh_sport	= client_port_number;
	udp->uh_dport	= server_port_number;
	udp->uh_ulen	= len;
	udp->uh_sum		= 0;

	/* The following initialization is needed for the UDP
	 * datagram checksum to be calculated correctly. The
	 * checksum is calculated using information found in
	 * the IPv4 header, which is partly filled in here, too.
	 */
	udp_pseudo_header = (struct udp_pseudo_header *)ip;
	udp_pseudo_header->ip_zero1[0] = udp_pseudo_header->ip_zero1[1] = 0;
	udp_pseudo_header->ip_zero2 = 0;

	udp_pseudo_header->ip_pr	= IPPROTO_UDP;
	udp_pseudo_header->ip_src	= local_ipv4_address;
	udp_pseudo_header->ip_dst	= remote_ipv4_address;
	udp_pseudo_header->ip_len	= udp_pseudo_header->uh_ulen;
	
	udp->uh_sum = in_cksum(ip,sizeof(*ip) + udp_pseudo_header->uh_ulen);
	
	/*
	 * Set up the IPv4 header and its checksum.
	 */

	len += sizeof(*ip);

	ip->ip_v_hl	= (IPVERSION << 4) | 5;
	ip->ip_len	= len;
	ip->ip_ttl	= 64;
	ip->ip_pr	= IPPROTO_UDP;
	ip->ip_sum	= 0;
	ip->ip_sum	= in_cksum(ip, sizeof(*ip));

	ASSERT( len <= write_request->nior_BufferSize );

	write_request->nior_IOS2.ios2_Req.io_Command	= CMD_WRITE;
	write_request->nior_IOS2.ios2_WireError			= 0;
	write_request->nior_IOS2.ios2_PacketType		= ETHERTYPE_IP;
	write_request->nior_IOS2.ios2_Data				= write_request;
	write_request->nior_IOS2.ios2_DataLength		= len;

	memmove(write_request->nior_IOS2.ios2_DstAddr,remote_ethernet_address,sizeof(remote_ethernet_address));

	#if defined(TESTING)
	{
		if(0 < drop_tx && (rand() % 100) < drop_tx)
		{
			Printf("TESTING: Dropping IP datagram.\n");

			return(0);
		}
		else if (0 < trash_tx && (rand() % 100) < trash_tx)
		{
			if(write_request->nior_IOS2.ios2_DataLength > 0)
			{
				Printf("TESTING: Trashing IP datagram.\n");

				ASSERT( write_request->nior_IOS2.ios2_DataLength <= write_request->nior_BufferSize );

				((UBYTE *)write_request->nior_Buffer)[rand() % write_request->nior_IOS2.ios2_DataLength] ^= 0x81;
			}
		}
	}
	#endif /* TESTING */

	ASSERT( NOT write_request->nior_InUse );

	error = DoIO((struct IORequest *)write_request);

	RETURN(error);
	return(error);
}

/****************************************************************************/

/* Verify that the checksum of the UDP datagram is correct, with respect
 * to the source and destination IPv4 addresses in the IP datagram
 * header, the UDP datagram header itself, and the UDP datagram payload.
 * If correct, the checksum should be 0.
 *
 * Please note that this function may write to the IP datagram header in order
 * to simplify the checksum calculation. The contents of the IP datagram
 * header will be restored after they have been changed, though.
 */
int
verify_udp_datagram_checksum(struct ip * ip)
{
	struct udphdr * udp = (struct udphdr *)&ip[1];
	int checksum;

	ASSERT( ip != NULL );

	/* If this UDP datagram has a checksum, verify it. */
	if(udp->uh_sum != 0)
	{
		struct udp_pseudo_header * udp_pseudo_header;
		struct ip ip_copy;

		/* We will clobber the IP header for the calculation,
		 * so let's save it first.
		 */
		ip_copy = (*ip);

		udp_pseudo_header = (struct udp_pseudo_header *)ip;
		udp_pseudo_header->ip_zero1[0] = udp_pseudo_header->ip_zero1[1] = 0;
		udp_pseudo_header->ip_zero2 = 0;
		udp_pseudo_header->ip_len = udp_pseudo_header->uh_ulen;

		checksum = in_cksum(ip,sizeof(*ip) + udp_pseudo_header->uh_ulen);

		/* Restore the damage. */
		(*ip) = ip_copy;
	}
	/* Otherwise accept it as is. */
	else
	{
		checksum = 0;
	}

	return(checksum);
}

/****************************************************************************/

/* This function processes a path name such as "192.168.0.1:/tftpboot/file.txt"
 * and splits it into an IPv4 address (192.168.0.1) and the file name following
 * it (/tftpboot/file.txt). The IPv4 address is then processed and returned
 * separately from the file name. If no valid IPv4 address is found, then the
 * file name will be returned as is.
 */
void
get_ipv4_address_and_path_from_name(STRPTR name, ULONG * ipv4_address, STRPTR * path_name)
{
	STRPTR s;

	ASSERT( name != NULL );
	ASSERT( ipv4_address != NULL );
	ASSERT( path_name != NULL );

	/* If no IPv4 can be found as part of the name, then we return
	 * just the name as is, and no IPv4 address.
	 */
	(*path_name) = name;
	(*ipv4_address) = 0;

	/* Check if the name begins with a volume name, or a separator
	 * with the IPv4 address in front of it.
	 */
	s = strchr(name, ':');
	if(s != NULL)
	{
		size_t address_len = (size_t)(s - name);
		char address_part[40];

		if(address_len < sizeof(address_part))
		{
			ULONG v;

			/* Copy what could be the IPv4 address in front
			 * of the separator character.
			 */
			memmove(address_part,name,address_len);
			address_part[address_len] = '\0';

			/* If it was a valid IPv4 address (as far as the
			 * syntax is concerned), return the address and
			 * the path name following the address
			 * separately.
			 */
			if(inet_aton(address_part, &v))
			{
				(*path_name) = s+1;
				(*ipv4_address) = v;
			}
		}
	}
}
