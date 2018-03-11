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

/****************************************************************************/

#define __USE_INLINE__
#include <proto/exec.h>
#include <proto/dos.h>

/****************************************************************************/

#include "testing.h"
#include "network-io.h"
#include "network-arp.h"

/****************************************************************************/

#include "assert.h"

/****************************************************************************/

/* Send an ARP response message, which in this case means that we report which
 * combination of Ethernet address and IPv4 address this command uses. This sends
 * a single packet to the computer which broadcast an ARP request message.
 */
LONG
send_arp_response(ULONG target_ipv4_address,const UBYTE * target_ethernet_address)
{
	struct ARPHeaderEthernet * ahe = write_request->nior_Buffer;

	ASSERT( sizeof(*ahe) <= write_request->nior_BufferSize );

	memset(ahe,0,sizeof(*ahe));

	ahe->ahe_HardwareAddressFormat	= ARPHRD_ETHER;
	ahe->ahe_ProtocolAddressFormat	= ETHERTYPE_IP;
	ahe->ahe_HardwareAddressLength	= sizeof(ahe->ahe_SenderHardwareAddress);
	ahe->ahe_ProtocolAddressLength	= sizeof(ahe->ahe_SenderProtocolAddress);
	ahe->ahe_Operation				= ARPOP_REPLY;

	/* This is our local combination of Ethernet & IPv4 address. */
	memmove(ahe->ahe_SenderHardwareAddress,local_ethernet_address,sizeof(ahe->ahe_SenderHardwareAddress));
	ahe->ahe_SenderProtocolAddress = local_ipv4_address;

	/* This is the combination of Ethernet & IPv4 address of the system which
	 * sent the ARP request.
	 */
	memmove(ahe->ahe_TargetHardwareAddress,target_ethernet_address,sizeof(ahe->ahe_TargetHardwareAddress));
	ahe->ahe_TargetProtocolAddress = target_ipv4_address;

	write_request->nior_IOS2.ios2_Req.io_Command	= CMD_WRITE;
	write_request->nior_IOS2.ios2_WireError			= 0;
	write_request->nior_IOS2.ios2_PacketType		= ETHERTYPE_ARP;
	write_request->nior_IOS2.ios2_Data				= write_request;
	write_request->nior_IOS2.ios2_DataLength		= sizeof(*ahe);

	memmove(write_request->nior_IOS2.ios2_DstAddr,ahe->ahe_TargetHardwareAddress,6);

	/* If the test mode is built into this command, the following lines will
	 * randomly drop packets instead of transmitting them, or corrupt their
	 * contents before transmitting them.
	 */
	#if defined(TESTING)
	{
		if(0 < drop_tx && (rand() % 100) < drop_tx)
		{
			Printf("TESTING: Dropping ARP response.\n");

			return(0);
		}
		else if (0 < trash_tx && (rand() % 100) < trash_tx)
		{
			if(write_request->nior_IOS2.ios2_DataLength > 0)
			{
				Printf("TESTING: Trashing ARP response.\n");

				ASSERT( write_request->nior_IOS2.ios2_DataLength <= write_request->nior_BufferSize );

				((UBYTE *)write_request->nior_Buffer)[rand() % write_request->nior_IOS2.ios2_DataLength] ^= 0x81;
			}
		}
	}
	#endif /* TESTING */

	return(DoIO((struct IORequest *)write_request));
}

/****************************************************************************/

/* Broadcast an ARP request message, asking for the Ethernet address to be
 * reported which corresponds to the IPv4 address of the system which we want
 * to send UDP datagrams to.
 */
LONG
broadcast_arp_query(ULONG target_ipv4_address)
{
	struct ARPHeaderEthernet * ahe = write_request->nior_Buffer;
	LONG error;

	ENTER();

	ASSERT( sizeof(*ahe) <= write_request->nior_BufferSize );

	memset(ahe,0,sizeof(*ahe));

	ahe->ahe_HardwareAddressFormat	= ARPHRD_ETHER;
	ahe->ahe_ProtocolAddressFormat	= ETHERTYPE_IP;
	ahe->ahe_HardwareAddressLength	= sizeof(ahe->ahe_SenderHardwareAddress);
	ahe->ahe_ProtocolAddressLength	= sizeof(ahe->ahe_SenderProtocolAddress);
	ahe->ahe_Operation				= ARPOP_REQUEST;

	/* This is our local combination of Ethernet & IPv4 address. */
	memmove(ahe->ahe_SenderHardwareAddress,local_ethernet_address,sizeof(ahe->ahe_SenderHardwareAddress));
	ahe->ahe_SenderProtocolAddress = local_ipv4_address;

	/* We don't know the Ethernet address corresponding to the TFTP server
	 * IPv4 address, so we will send it with the Ethernet broadcast address.
	 */
	memset(ahe->ahe_TargetHardwareAddress,0xff,sizeof(ahe->ahe_TargetHardwareAddress));
	ahe->ahe_TargetProtocolAddress = remote_ipv4_address;

	write_request->nior_IOS2.ios2_Req.io_Command	= S2_BROADCAST;
	write_request->nior_IOS2.ios2_WireError			= 0;
	write_request->nior_IOS2.ios2_PacketType		= ETHERTYPE_ARP;
	write_request->nior_IOS2.ios2_Data				= write_request;
	write_request->nior_IOS2.ios2_DataLength		= sizeof(*ahe);

	/* If the test mode is built into this command, the following lines will
	 * randomly drop packets instead of transmitting them, or corrupt their
	 * contents before transmitting them.
	 */
	#if defined(TESTING)
	{
		if(0 < drop_tx && (rand() % 100) < drop_tx)
		{
			Printf("TESTING: Dropping ARP query.\n");

			RETURN(0);
			return(0);
		}
		else if (0 < trash_tx && (rand() % 100) < trash_tx)
		{
			if(write_request->nior_IOS2.ios2_DataLength > 0)
			{
				Printf("TESTING: Trashing ARP query.\n");

				ASSERT( write_request->nior_IOS2.ios2_DataLength <= write_request->nior_BufferSize );

				((UBYTE *)write_request->nior_Buffer)[rand() % write_request->nior_IOS2.ios2_DataLength] ^= 0x81;
			}
		}
	}
	#endif /* TESTING */

	error = DoIO((struct IORequest *)write_request);

	RETURN(error);
	return(error);
}
