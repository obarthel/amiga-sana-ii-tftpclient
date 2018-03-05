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

#include <stddef.h>
#include <string.h>

/****************************************************************************/

#include <assert.h>

/****************************************************************************/

#include "network-tftp.h"
#include "network-ip-udp.h"

/****************************************************************************/

/* Send a TFTP acknowledgement packet to the remote server. Note that the
 * contents of the buffer pointed to by the tftp_packet parameter will be modified.
 */
LONG
send_tftp_acknowledgement(int block_number,int client_port_number,int server_port_number,UBYTE * tftp_packet)
{
	struct tftphdr * th = (struct tftphdr *)tftp_packet;

	th->th_opcode	= TFTP_PACKET_ACK;
	th->th_block	= block_number;

	return(send_udp(client_port_number,server_port_number,tftp_packet,(int)offsetof(struct tftphdr, th_data)));
}

/****************************************************************************/

/* Send a TFTP error packet to the remote server. Note that the contents of the buffer
 * pointed to by the tftp_packet parameter will be modified.
 */
LONG
send_tftp_error(int error_code,STRPTR message,int client_port_number,int server_port_number,UBYTE * tftp_packet)
{
	struct tftphdr * th = (struct tftphdr *)tftp_packet;
	char * msg = th->th_msg;

	th->th_opcode	= TFTP_PACKET_ERROR;
	th->th_code		= error_code;

	strcpy(msg,message);
	msg += strlen(msg)+1;

	return(send_udp(client_port_number,server_port_number,tftp_packet,(int)(msg - (char *)tftp_packet)));
}

/****************************************************************************/

/* Send a message with a request for the remote TFTP server begin the data transmission.
 * Note that the contents of the buffer pointed to by the tftp_packet parameter will be modified.
 */
LONG
start_tftp(int operation,STRPTR file_name,int client_port_number,int server_port_number,UBYTE * tftp_packet)
{
	struct tftphdr * th = (struct tftphdr *)tftp_packet;
	UBYTE * stuff;

	th->th_opcode = operation;

	stuff = th->th_stuff;

	strcpy(stuff,file_name);
	stuff += strlen(stuff)+1;

	strcpy(stuff,"octet");
	stuff += strlen(stuff)+1;

	return(send_udp(client_port_number,server_port_number,tftp_packet,(int)(stuff - tftp_packet)));
}
