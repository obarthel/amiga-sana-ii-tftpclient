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

#ifndef _NETWORK_IO_H
#define _NETWORK_IO_H

/****************************************************************************/

#ifndef DEVICES_SANA2_H
#include <devices/sana2.h>
#endif /* DEVICES_SANA2_H */

/****************************************************************************/

#ifndef _ARGS_H
#include "args.h"
#endif /* _ARGS_H */

/****************************************************************************/

#define	ETHERTYPE_IP	0x0800	/* IP protocol */
#define ETHERTYPE_ARP	0x0806	/* Address resolution protocol */

/****************************************************************************/

/* This is a standard IOSana2Req type IORequest with some extra data added on
 * top of it which helps in keeping track of which I/O request are currently
 * active, which ones are copies, and which memory buffer data is going into
 * or coming from.
 */
struct NetIORequest
{
	struct IOSana2Req	nior_IOS2;			/* Standard SANA-II I/O request */
	
	struct MinNode		nior_Link;			/* This keeps track of all NetIORequests allocated */

	UWORD				nior_Type;			/* Either ETHERTYPE_IP or ETHERTYPE_ARP */
	BOOL				nior_IsDuplicate;	/* False if device driver was opened with this IORequest, true otherwise */
	BOOL				nior_InUse;			/* True if used with SendIO() */

	APTR				nior_Buffer;		/* Address of transmission buffer */
	ULONG				nior_BufferSize;	/* Size of transmission buffer in bytes */
};

/****************************************************************************/

extern struct MsgPort * net_read_port;

/****************************************************************************/

extern struct NetIORequest * write_request;

/****************************************************************************/

extern UBYTE local_ethernet_address[SANA2_MAX_ADDR_BYTES];
extern ULONG local_ipv4_address;

extern UBYTE remote_ethernet_address[SANA2_MAX_ADDR_BYTES];
extern ULONG remote_ipv4_address;

/****************************************************************************/

extern void send_net_io_read_request(struct NetIORequest * nior,UWORD type);
extern void network_cleanup(void);
extern int network_setup(const struct cmd_args * args);

/****************************************************************************/

#endif /* _NETWORK_IO_H */
