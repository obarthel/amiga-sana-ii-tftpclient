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

#ifndef _NETWORK_ARP_H
#define _NETWORK_ARP_H

/****************************************************************************/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif /* EXEC_TYPES_H */

/****************************************************************************/

#define ARPHRD_ETHER	1	/* Ethernet hardware format */
#define	ARPOP_REQUEST	1	/* Request to resolve address */
#define	ARPOP_REPLY		2	/* Response to previous request */

struct ARPHeaderEthernet
{
	UWORD	ahe_HardwareAddressFormat;		/* Should be 1 for Ethernet */
	UWORD	ahe_ProtocolAddressFormat;		/* Should be 0x0800 for IPv4 */
	UBYTE	ahe_HardwareAddressLength;		/* Should be 6 for Ethernet */
	UBYTE	ahe_ProtocolAddressLength;		/* Should be 4 for IPv4 */
	UWORD	ahe_Operation;					/* 1 for request, 2 for reply */
	UBYTE	ahe_SenderHardwareAddress[6];
	ULONG	ahe_SenderProtocolAddress;
	UBYTE	ahe_TargetHardwareAddress[6];
	ULONG	ahe_TargetProtocolAddress;
};

/****************************************************************************/

extern LONG send_arp_response(ULONG target_ipv4_address,const UBYTE * target_ethernet_address);
extern LONG broadcast_arp_query(ULONG target_ipv4_address);

/****************************************************************************/

#endif /* _NETWORK_ARP_H */
