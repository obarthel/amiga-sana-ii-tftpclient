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

#ifndef _NETWORK_TFTP_H
#define _NETWORK_TFTP_H

/****************************************************************************/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif /* EXEC_TYPES_H */

/****************************************************************************/

/*
 * Trivial File Transfer Protocol (IEN-133).
 * Per RFC 1350, July, 1992
 */

#define TFTP_PORT_NUMBER 69

#define SEGSIZE 512	/* data segment size */

/* Packet types */
#define	TFTP_PACKET_RRQ		1	/* read request */
#define	TFTP_PACKET_WRQ		2	/* write request */
#define	TFTP_PACKET_DATA	3	/* data packet */
#define	TFTP_PACKET_ACK		4	/* acknowledgement */
#define	TFTP_PACKET_ERROR	5	/* error code */

struct tftphdr
{
	UWORD		th_opcode;		/* packet type */

	union
	{
		UWORD	tu_block;		/* block # */
		UWORD	tu_code;		/* error code */
		UBYTE	tu_stuff[1];	/* request packet stuff */
	} th_u;

	UBYTE		th_data[1];		/* data or error string */
};

#define	th_block	th_u.tu_block
#define	th_code		th_u.tu_code
#define	th_stuff	th_u.tu_stuff
#define	th_msg		th_data

/* Error codes */
#define	TFTP_ERROR_UNDEF	0	/* not defined */
#define	TFTP_ERROR_NOTFOUND	1	/* file not found */
#define	TFTP_ERROR_ACCESS	2	/* access violation */
#define	TFTP_ERROR_NOSPACE	3	/* disk full or allocation exceeded */
#define	TFTP_ERROR_BADOP	4	/* illegal TFTP operation */
#define	TFTP_ERROR_BADID	5	/* unknown transfer ID */
#define	TFTP_ERROR_EXISTS	6	/* file already exists */
#define	TFTP_ERROR_NOUSER	7	/* no such user */

/****************************************************************************/

extern LONG send_tftp_acknowledgement(int block_number,int client_port_number,int server_port_number,UBYTE * tftp_packet);
extern LONG send_tftp_error(int error_code,STRPTR message,int client_port_number,int server_port_number,UBYTE * tftp_packet);
extern LONG start_tftp(int operation,STRPTR file_name,int client_port_number,int server_port_number,UBYTE * tftp_packet);

/****************************************************************************/

#endif /* _NETWORK_TFTP_H */
