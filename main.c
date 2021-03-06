/*
 * :ts=4
 *
 * TFTP client program for the Amiga, using only the SANA-II network
 * device driver API, and no TCP/IP stack
 *
 * The "trivial file transfer protocol" is anything but trivial
 * to implement...
 *
 * Copyright � 2016 by Olaf Barthel <obarthel at gmx dot net>
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

// tftpclient device=ariadne_ii.device unit=0 verbose localaddress=192.168.1.230 from=192.168.1.116:/private/tftpboot/unix-tricks.txt to=unix-tricks.txt
// tftpclient device=ariadne_ii.device unit=0 verbose localaddress=192.168.1.230 from=unix-tricks.txt to=192.168.1.116:/private/tftpboot/write-test

#include <devices/timer.h>

#include <exec/memory.h>
#include <devices/sana2.h>

#include <dos/rdargs.h>
#include <dos/stdio.h>

/****************************************************************************/

#define __USE_INLINE__
#include <proto/utility.h>
#include <proto/exec.h>
#include <proto/dos.h>

/****************************************************************************/

#if defined(__amigaos4__)
#include <dos/obsolete.h>
#endif /* __amigaos4__ */

/****************************************************************************/

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/****************************************************************************/

#include "macros.h"

#include "network-io.h"
#include "network-arp.h"
#include "network-ip-udp.h"
#include "network-tftp.h"

#include "error-codes.h"
#include "testing.h"
#include "timer.h"
#include "args.h"

/****************************************************************************/

#include "assert.h"

/****************************************************************************/

#include "TFTPClient_rev.h"

#ifndef TESTING
const char VersionTag[] = VERSTAG;
#else
const char VersionTag[] = VERSTAG " TESTING";
#endif /* !TESTING */

/****************************************************************************/

struct Library * UtilityBase;

/****************************************************************************/

#if defined(__amigaos4__)

struct UtilityIFace * IUtility;

#endif /* __amigaos4__ */

/****************************************************************************/

/* This function stops all I/O operations and releases all the resources
 * allocated by the setup() function. It is safe to call even if setup()
 * was never called, or setup() was only partially successful, or if
 * cleanup() has already been called.
 */
static void
cleanup(void)
{
	ENTER();

	network_cleanup();
	timer_cleanup();
	
	#if defined(__amigaos4__)
	{
		if(IUtility != NULL)
		{
			DropInterface((struct Interface *)IUtility);
			IUtility = NULL;
		}
	}
	#endif /* __amigaos4__ */

	if(UtilityBase != NULL)
	{
		CloseLibrary(UtilityBase);
		UtilityBase = NULL;
	}

	LEAVE();
}

/****************************************************************************/

/* Initialize the network I/O operations, allocating SANA-II I/O requests,
 * opening the network device driver, the timer, etc. Returns 0 on success,
 * and -1 on failure.
 */
static int
setup(BPTR error_output, const struct cmd_args * args)
{
	int result = FAILURE;

	ENTER();

	atexit(cleanup);

	/* We would like to use the utility.library V39 function GetUniqueID().
	 * If it's not available, we'll use a different approach instead. So
	 * there is no need to succeed in opening utility.library V39.
	 */
	UtilityBase = OpenLibrary("utility.library", 39);

	#if defined(__amigaos4__)
	{
		if(UtilityBase != NULL)
		{
			IUtility = (struct UtilityIFace *)GetInterface(UtilityBase, "main", 1, 0);
			if(IUtility == NULL)
			{
				CloseLibrary(UtilityBase);
				UtilityBase = NULL;
			}
		}
	}
	#endif /* __amigaos4__ */

	if(timer_setup(error_output,args) != OK)
		goto out;

	if(network_setup(error_output, args) != OK)
		goto out;

	result = OK;

 out:

	RETURN(result);
	return(result);
}

/****************************************************************************/

int
main(int argc,char ** argv)
{
	/* The TFTP exchange rattles through the following
	 * steps until the transmission has completed or an
	 * error has occured.
	 */
	enum tftp_state_t
	{
		tftp_state_request_ethernet_address,
		tftp_state_request_read,
		tftp_state_request_write,
		tftp_state_write_to_file,
		tftp_state_read_from_file,
	};

	enum tftp_state_t tftp_state = tftp_state_request_ethernet_address;

	struct RDArgs * rda = NULL;
	int result = RETURN_FAIL;
	struct cmd_args args;
	TEXT device_name[256];
	LONG device_unit;
	TEXT local_ip_address[20];
	STRPTR local_filename;
	STRPTR remote_filename;
	ULONG from_ipv4_address;
	STRPTR from_path;
	ULONG to_ipv4_address;
	STRPTR to_path = NULL;
	ULONG signals_received;
	ULONG time_signal_mask, net_signal_mask, signal_mask;
	int num_arp_resolution_attempts = 4;
	BPTR source_file = (BPTR)NULL;
	BPTR destination_file = (BPTR)NULL;
	BOOL delete_destination_file = FALSE;
	int client_udp_port_number;
	int server_udp_port_number = TFTP_PORT_NUMBER;
	BOOL server_udp_port_number_known = FALSE;
	UBYTE tftp_packet[2 * sizeof(UWORD) + SEGSIZE]; /* opcode and block, followed by data */
	struct tftphdr * tftp_output = (struct tftphdr *)tftp_packet;
	int tftp_output_length = 0;
	int tftp_payload_length = 0;
	int block_number = 1;
	BOOL last_block_transmitted = FALSE;
	int num_eof_acknowledgements = 3;
	LONG total_num_bytes_transferred = 0;
	const struct Process * this_process = (struct Process *)FindTask(NULL);
	BPTR error_output = this_process->pr_CES != (BPTR)NULL ? this_process->pr_CES : Output();
	char ipv4_address[20];
	const char * from_computer;
	const char * to_computer;

	SETDEBUGLEVEL(DEBUGLEVEL_CallTracing);

	memset(&args,0,sizeof(args));

	if(((struct Library *)DOSBase)->lib_Version < 37)
	{
		char * error_message = "This program requires AmigaOS 2.04 or better.\n";

		Write(Output(),error_message,strlen(error_message));
		goto out;
	}

	/* Process the command line parameters. */
	rda = ReadArgs(cmd_template,(LONG *)&args,NULL);
	if(rda == NULL)
	{
		PrintFault(IoErr(),"TFTPClient");
		goto out;
	}

	if(args.Quiet)
		args.Verbose = FALSE;

	if(args.Verbose)
		args.Quiet = FALSE;

	/* If no DEVICE argument was provided, try the "TFTPDEVICE" environment variable. */
	if(args.DeviceName == NULL)
	{
		if(GetVar("TFTPDEVICE",device_name,sizeof(device_name),0) > 0)
			args.DeviceName = device_name;
	}

	if(args.DeviceName == NULL)
	{
		if(!args.Quiet)
			FPrintf(error_output, "%s: Required %s argument is missing.\n","TFTPClient", "DEVICENAME");

		goto out;
	}

	/* If no UNIT argument was provided, try the "TFTPUNIT" environment variable. */
	if(args.DeviceUnit == NULL)
	{
		TEXT value[16];

		if(GetVar("TFTPUNIT",value,sizeof(value),0) > 0)
		{
			if(StrToLong(value,&device_unit) > 0)
				args.DeviceUnit = &device_unit;
		}
	}

	if(args.DeviceUnit == NULL)
	{
		device_unit = 0;

		args.DeviceUnit = &device_unit;
	}

	/* If no LOCALADDRESS argument was provided, try the "TFTPLOCALADDRESS" environment variable. */
	if(args.LocalIPAddress == NULL)
	{
		if(GetVar("TFTPLOCALADDRESS",local_ip_address,sizeof(local_ip_address),0) > 0)
			args.LocalIPAddress = local_ip_address;
	}

	if(args.LocalIPAddress == NULL)
	{
		if(!args.Quiet)
			FPrintf(error_output, "%s: Required %s argument is missing.\n","TFTPClient", "LOCALADDRESS");

		goto out;
	}

	/* The local IP address should be given in one of the
	 * standard IPv4 formats. Some minimal filtering of
	 * unusable addresses is performed here, too.
	 */
	if(!inet_aton(args.LocalIPAddress,&local_ipv4_address) || local_ipv4_address == 0 ||
	   local_ipv4_address == 0xFFFFFFFFUL || local_ipv4_address == 0x7F000001)
	{
		if(!args.Quiet)
			FPrintf(error_output, "%s: '%s' is not a valid local IPv4 address.\n","TFTPClient",args.LocalIPAddress);

		goto out;
	}

	if(args.RemotePort != NULL)
	{
		LONG remote_port = (*args.RemotePort);

		if(remote_port < 0 || remote_port > 65535)
		{
			if(!args.Quiet)
				FPrintf(error_output, "%s: Remote port number %ld is out of range; valid range is 1..65535, default is 69.\n","TFTPClient",remote_port);

			goto out;
		}

		server_udp_port_number = remote_port;
	}

	/* The source is either a file name ("example"), or a file name with
	 * an IPv4 address prefix, like a device name ("192.168.0.1:example").
	 */
	get_ipv4_address_and_path_from_name(args.Source, &from_ipv4_address, &from_path);

	if(from_ipv4_address == local_ipv4_address || from_ipv4_address == 0x7F000001)
		from_ipv4_address = 0;

	/* The file name is mandatory. */
	if((*from_path) == '\0')
	{
		if(!args.Quiet)
			FPrintf(error_output, "%s: FROM argument \"%s\" must include a file name.\n","TFTPClient", args.Source);

		goto out;
	}

	/* The destination is either a file name ("example"), or a file name with
	 * an IPv4 address prefix, like a device name ("192.168.0.1:example").
	 */
	get_ipv4_address_and_path_from_name(args.Destination, &to_ipv4_address, &to_path);

	if((*to_path) == '\0')
		to_path = (STRPTR)FilePart(from_path);

	if(to_ipv4_address == local_ipv4_address || to_ipv4_address == 0x7F000001)
		to_ipv4_address = 0;

	if(to_ipv4_address == 0xFFFFFFFFUL)
	{
		if(!args.Quiet)
			FPrintf(error_output, "%s: '%s' is not a valid IPv4 destination address.\n","TFTPClient",args.Destination);

		goto out;
	}

	if(from_ipv4_address != 0 && to_ipv4_address != 0)
	{
		if(!args.Quiet)
			FPrintf(error_output, "%s: Please provide a source or destination IPv4 address, but not both.\n","TFTPClient");

		goto out;
	}

	if(from_ipv4_address == to_ipv4_address)
	{
		if(!args.Quiet)
			FPrintf(error_output, "%s: Please provide either a source or destination IPv4 address.\n","TFTPClient");

		goto out;
	}

	if(from_ipv4_address != 0)
	{
		local_filename		= to_path;
		remote_ipv4_address	= from_ipv4_address;
		remote_filename		= from_path;
	}
	else
	{
		local_filename		= from_path;
		remote_ipv4_address	= to_ipv4_address;
		remote_filename		= to_path;
	}

	/* Make sure that the file name is not too long to be transmitted safely. */
	if(offsetof(struct tftphdr, th_data) + strlen(remote_filename) + 1 + strlen("octet") + 1 > sizeof(tftp_packet))
	{
		if(!args.Quiet)
		{
			FPrintf(error_output, "%s: File name \"%s\" is too long (up to %ld characters are allowed).\n","TFTPClient",
				remote_filename,sizeof(tftp_packet) - (offsetof(struct tftphdr, th_data) + 1 + strlen("octet") + 1));
		}

		goto out;
	}

	/* The arguments check out, now set up the network and timer I/O. */
	if(setup(error_output, &args) < 0)
		goto out;

	if(from_ipv4_address == 0)
	{
		sprintf(ipv4_address,"%lu.%lu.%lu.%lu",
			(to_ipv4_address >> 24) & 0xff,
			(to_ipv4_address >> 16) & 0xff,
			(to_ipv4_address >>  8) & 0xff,
			 to_ipv4_address        & 0xff);

		from_computer	= "this computer";
		to_computer		= ipv4_address;
	}
	else
	{
		sprintf(ipv4_address,"%lu.%lu.%lu.%lu",
			(from_ipv4_address >> 24) & 0xff,
			(from_ipv4_address >> 16) & 0xff,
			(from_ipv4_address >>  8) & 0xff,
			 from_ipv4_address        & 0xff);

		from_computer	= ipv4_address;
		to_computer		= "this computer";
	}

	if(args.Verbose)
	{
		Printf("Copy \"%s\" (%s) to \"%s\" (%s)\n",
			from_path,
			from_computer,
			to_path,
			to_computer);
	}

	D(("Will copy \"%s\" (%s) to \"%s\" (%s).",
		from_path,
		from_computer,
		to_path,
		to_computer
	));

	/* We are sending a file? */
	if(from_ipv4_address == 0)
	{
		source_file = Open(from_path, MODE_OLDFILE);
		if(source_file == (BPTR)NULL)
		{
			TEXT error_message[256];

			Fault(IoErr(),NULL,error_message,sizeof(error_message));

			if(!args.Quiet)
				FPrintf(error_output, "%s: Could not open file \"%s\" for reading (%s).\n","TFTPClient",from_path,error_message);

			D(("Could not open file '%s' for reading (%s).",from_path,error_message));

			goto out;
		}

		D(("Opened '%s' for reading.", from_path));

		/* Use a read buffer. */
		SetVBuf(source_file,NULL,BUF_FULL,8192);
	}
	/* We are receiving a file. */
	else
	{
		/* Unless specifically requested, we verify that the
		 * file to write to does not yet exist and exit with
		 * error if does.
		 */
		if(!args.Overwrite)
		{
			BPTR test_lock;

			test_lock = Lock(to_path, EXCLUSIVE_LOCK);
			if(test_lock == (BPTR)NULL)
			{
				/* It's acceptable if the file in question does
				 * not exist yet. But everything else we count
				 * as an error, report it and abort.
				 */
				if(IoErr() != ERROR_OBJECT_NOT_FOUND)
				{
					TEXT error_message[256];

					Fault(IoErr(),NULL,error_message,sizeof(error_message));

					if(!args.Quiet)
						FPrintf(error_output, "%s: Could not check if file \"%s\" exists (%s).\n","TFTPClient",to_path,error_message);

					D(("Could not check if file '%s' exists (%s).",to_path,error_message));

					goto out;
				}
			}
			/* The file already exists. */
			else
			{
				UnLock(test_lock);

				if(!args.Quiet)
					Printf("%s: Destination file \"%s\" already exists. Use OVERWRITE argument to replace it.\n","TFTPClient",to_path);

				D(("Destination file '%s' already exists.",to_path));

				result = RETURN_WARN;
				goto out;
			}
		}

		/* This will either create or overwrite the file. */
		destination_file = Open(to_path, MODE_NEWFILE);
		if(destination_file == (BPTR)NULL)
		{
			TEXT error_message[256];

			Fault(IoErr(),NULL,error_message,sizeof(error_message));

			if(!args.Quiet)
				FPrintf(error_output, "%s: Could not open file \"%s\" for writing (%s).\n","TFTPClient",to_path,error_message);

			D(("Could not open file '%s' for writing (%s).",to_path,error_message));

			goto out;
		}
		
		D(("Opened '%s' for writing.", to_path));

		/* Use a write buffer. */
		SetVBuf(destination_file,NULL,BUF_FULL,8192);

		/* Delete an empty file. */
		delete_destination_file = TRUE;
	}

	/* We need to send UDP datagrams using a specific port number, which
	 * uniquely identifies the TFTP session. This picks an "ephemeral"
	 * port number, which should be in the range 49152..65535.
	 */
	if(UtilityBase != NULL && UtilityBase->lib_Version >= 39)
	{
		/* Really use a unique ID. */
		client_udp_port_number = 49152 + (GetUniqueID() % 16384);
	}
	else
	{
		/* Try to get by with a pseudo-randomly chosen port. */
		srand(time(NULL));

		client_udp_port_number = 49152 + (rand() % 16384);
	}

	D(("client udp port number = %ld", client_udp_port_number));

	if(args.Verbose)
		Printf("Sending ARP query...\n");

	SHOWMSG("Sending ARP query.");

	/* We need to know the Ethernet address corresponding to the IPv4
	 * address of the remote TFTP server.
	 */
	broadcast_arp_query(remote_ipv4_address);

	D(("starting the timer"));

	start_time(1);

	time_signal_mask	= (1UL << time_port->mp_SigBit);
	net_signal_mask		= (1UL << net_read_port->mp_SigBit);

	signal_mask = SIGBREAKF_CTRL_C | time_signal_mask | net_signal_mask;
	signals_received = 0;

	while(TRUE)
	{
		/* Wait for something to happen... */
		if(signals_received == 0)
			signals_received = Wait(signal_mask);
		/* Keep processing the signals set by previous Wait(),
		 * polling for new signal events.
		 */
		else
			signals_received |= (SetSignal(0,signal_mask) & signal_mask);

		/* Stop the program? */
		if(signals_received & SIGBREAKF_CTRL_C)
			break;

		/* New network data has arrived? */
		if(signals_received & net_signal_mask)
		{
			struct NetIORequest * read_request;

			/* Pick up the next network I/O request available, then
			 * mark it as no longer in use.
			 */
			read_request = (struct NetIORequest *)GetMsg(net_read_port);
			
			if(read_request != NULL)
				D(("Read request 0x%08lx has returned.", read_request));

			#if defined(TESTING)
			{
				if(read_request != NULL)
				{
					const char * type = (read_request->nior_Type == ETHERTYPE_IP) ? "IP" : "ARP";

					read_request->nior_InUse = FALSE;

					if(0 < drop_rx && (rand() % 100) < drop_rx)
					{
						Printf("TESTING: Dropping received %s packet.\n", type);

						D(("TESTING: Dropping received %s packet.", type));

						send_net_io_read_request(read_request,read_request->nior_Type);
						read_request = NULL;
					}
					else if (0 < trash_rx && (rand() % 100) < trash_rx)
					{
						if(read_request->nior_IOS2.ios2_DataLength > 0)
						{
							Printf("TESTING: Trashing received %s packet.\n", type);

							D(("TESTING: Trashing received %s packet.", type));

							ASSERT( read_request->nior_IOS2.ios2_DataLength <= read_request->nior_BufferSize );

							((UBYTE *)read_request->nior_Buffer)[rand() % read_request->nior_IOS2.ios2_DataLength] ^= 0x81;
						}
					}
				}
			}
			#endif /* TESTING */

			if(read_request != NULL)
			{
				read_request->nior_InUse = FALSE;

				/* Is this an IP datagram? */
				if (read_request->nior_Type == ETHERTYPE_IP)
				{
					struct ip * ip = read_request->nior_Buffer;

					SHOWMSG("received an IP datagram");

					/* Verify that the IP header checksum is correct. */
					if(read_request->nior_IOS2.ios2_DataLength >= sizeof(*ip) && in_cksum(ip,sizeof(*ip)) == 0)
					{
						/* This should be an IPv4 datagram, and it should contain
						 * an UDP datagram.
						 */
						if(((ip->ip_v_hl >> 4) & 15) == IPVERSION && ip->ip_pr == IPPROTO_UDP)
						{
							struct udphdr * udp = (struct udphdr *)&ip[1];
							int checksum;

							SHOWMSG("datagram contains UDP data");

							/* Is the UDP datagram checksum OK, and the datagram is
							 * intended for our use? Furthermore, are we even ready to
							 * process it yet?
							 */
							checksum = verify_udp_datagram_checksum(ip);

							if(checksum == 0 &&
							   udp->uh_dport == client_udp_port_number &&
							   (!server_udp_port_number_known || udp->uh_sport == server_udp_port_number) &&
							   tftp_state > tftp_state_request_ethernet_address)
							{
								const struct tftphdr * tftp = (struct tftphdr *)&udp[1];
								int length = udp->uh_ulen - sizeof(*udp);

								/* Server responded with an error? We print the error message and abort. */
								if (tftp->th_opcode == TFTP_PACKET_ERROR)
								{
									const char * error_text;
									char number[20];
									char message_buffer[SEGSIZE+1];
									int message_length;

									SHOWMSG("TFTP opcode = TFTP_PACKET_ERROR");

									error_text = get_tftp_error_text(tftp->th_code);
									if(error_text == NULL)
									{
										sprintf(number,"error %d",tftp->th_code);
										error_text = number;
									}

									message_length = length - offsetof(struct tftphdr, th_msg);

									ASSERT( message_length >= 0 );
									ASSERT( message_length < (int)sizeof(message_buffer) );

									memmove(message_buffer,tftp->th_msg,message_length);
									message_buffer[message_length] = '\0';

									if(!args.Quiet)
										FPrintf(error_output, "%s: Server responded with error '%s', \"%s\".\n","TFTPClient",error_text,message_buffer);

									D(("Server responded with error '%s', '%s'.",error_text,message_buffer));

									result = RETURN_ERROR;
									goto out;
								}
								/* Server has transmitted data? */
								else if (tftp->th_opcode == TFTP_PACKET_DATA)
								{
									int payload_length = length - offsetof(struct tftphdr, th_data);

									SHOWMSG("TFTP opcode = TFTP_PACKET_DATA");

									/* Make sure that the data packet size is sane. */
									if(payload_length > SEGSIZE)
									{
										if(args.Verbose)
										{
											Printf("Data packet size (%ld bytes) is larger than expected; keeping only the first %ld bytes.\n",
												payload_length, SEGSIZE);
										}
										
										D(("Data packet size (%ld bytes) is larger than expected; keeping only the first %ld bytes.",payload_length, SEGSIZE));

										payload_length = SEGSIZE;
									}

									/* Did we just request to start reception of data? */
									if (tftp_state == tftp_state_request_read)
									{
										/* This should be the very first data block. */
										if(tftp->th_block == 1)
										{
											/* This is important: the server's tftp session is bound
											 * to a specific port number now.
											 */
											server_udp_port_number = udp->uh_sport;
											server_udp_port_number_known = TRUE;

											if(args.Verbose)
												Printf("Server has acknowledged the read request (using UDP port number %ld).\n", server_udp_port_number);

											D(("Server has acknowledged the read request (using UDP port number %ld).", server_udp_port_number));

											/* Store the data, if any. */
											if(payload_length > 0)
											{
												if(args.Verbose)
													Printf("Writing block #%ld (%ld bytes).\n",tftp->th_block,payload_length);

												D(("Writing block #%ld (%ld bytes).",tftp->th_block,payload_length));

												SetIoErr(0);

												if(FWrite(destination_file,(APTR)tftp->th_data,payload_length,1) == 0)
												{
													TEXT error_message[256];

													Fault(IoErr(),NULL,error_message,sizeof(error_message));

													if(!args.Quiet)
														FPrintf(error_output, "%s: Error writing to file \"%s\" (%s).\n","TFTPClient",to_path,error_message);

													D(("Error writing to file '%s' (%s).",to_path,error_message));

													send_tftp_error(TFTP_ERROR_UNDEF,"Error writing to file",client_udp_port_number,server_udp_port_number,tftp_packet);

													result = RETURN_ERROR;
													goto out;
												}

												total_num_bytes_transferred += payload_length;

												/* We received some data to keep, so do not delete the file. */
												delete_destination_file = FALSE;
											}
											else
											{
												SHOWMSG("no data in the packet");
											}

											/* Is this the last data to be received? */
											if(payload_length < SEGSIZE)
											{
												SHOWMSG("this is the last block transmitted by the server");

												last_block_transmitted = TRUE;
												num_eof_acknowledgements--;
											}

											tftp_state = tftp_state_write_to_file;

											block_number = 2;

											if(args.Verbose)
												Printf("Acknowledging receipt of block #%ld.\n",block_number-1);

											D(("Acknowledging receipt of block #%ld.",block_number-1));

											send_tftp_acknowledgement(block_number-1,client_udp_port_number,server_udp_port_number,tftp_packet);

											/* Restart the timer; make sure that we won't try to
											 * service the returning time request or there will
											 * be trouble.
											 */
											signals_received &= ~time_signal_mask;

											D(("starting the timer"));

											start_time(1);
										}
										else
										{
											if(args.Verbose)
												Printf("Ignoring receipt of block #%ld.\n",tftp->th_block);
											
											D(("Ignoring receipt of block #%ld.",tftp->th_block));
										}
									}
									/* Are we already receiving data to be written? */
									else if (tftp_state == tftp_state_write_to_file)
									{
										/* Is this the next block we expected? */
										if(tftp->th_block == block_number)
										{
											/* Store the data, if any. */
											if(payload_length > 0)
											{
												if(args.Verbose)
													Printf("Writing block #%ld (%ld bytes).\n",tftp->th_block,payload_length);

												D(("Writing block #%ld (%ld bytes).",tftp->th_block,payload_length));

												SetIoErr(0);

												if(FWrite(destination_file,(APTR)tftp->th_data,payload_length,1) == 0)
												{
													TEXT error_message[256];

													Fault(IoErr(),NULL,error_message,sizeof(error_message));

													if(!args.Quiet)
														FPrintf(error_output, "%s: Error writing to file \"%s\" (%s).\n","TFTPClient",to_path,error_message);
													
													D(("Error writing to file '%s' (%s).",to_path,error_message));

													send_tftp_error(TFTP_ERROR_UNDEF,"Error writing to file",client_udp_port_number,server_udp_port_number,tftp_packet);

													result = RETURN_ERROR;
													goto out;
												}

												total_num_bytes_transferred += payload_length;

												/* We received some data to keep, do not delete the file. */
												delete_destination_file = FALSE;
											}

											/* Is this the last data to be received? */
											if(payload_length < SEGSIZE)
											{
												last_block_transmitted = TRUE;
												num_eof_acknowledgements--;
											}

											block_number++;

											if(args.Verbose)
												Printf("Acknowledging receipt of block #%ld.\n",block_number-1);
											
											D(("Acknowledging receipt of block #%ld.",block_number-1));

											send_tftp_acknowledgement(block_number-1,client_udp_port_number,server_udp_port_number,tftp_packet);

											/* Restart the timer; make sure that we won't try to
											 * service the returning time request or there will
											 * be trouble.
											 */
											signals_received &= ~time_signal_mask;

											D(("starting the timer"));

											start_time(1);
										}
										/* No, this is the wrong block. */
										else
										{
											if(args.Verbose)
												Printf("Ignoring receipt of block #%ld; was expecting block #%ld instead.\n",tftp->th_block,block_number);
											
											D(("Ignoring receipt of block #%ld; was expecting block #%ld instead.",tftp->th_block,block_number));
										}
									}
									else
									{
										if(args.Verbose)
											Printf("Ignoring receipt of unexpected data block #%ld.\n",tftp->th_block);
										
										D(("Ignoring receipt of unexpected data block #%ld.",tftp->th_block));
									}
								}
								/* Server has acknowledged reception of data, or of the write request? */
								else if (tftp->th_opcode == TFTP_PACKET_ACK)
								{
									SHOWMSG("TFTP opcode = TFTP_PACKET_ACK");

									/* Could this be the server response to the write request? */
									if (tftp_state == tftp_state_request_write)
									{
										/* The acknowledgement comes in the form of block #0 only. */
										if(tftp->th_block == 0)
										{
											LONG num_bytes_read;

											/* This is important: the server's tftp session is bound
											 * to a specific port number now.
											 */
											server_udp_port_number = udp->uh_sport;
											server_udp_port_number_known = TRUE;

											if(args.Verbose)
												Printf("Server has acknowledged the write request (using UDP port number %ld).\n", server_udp_port_number);
											
											D(("Server has acknowledged the write request (using UDP port number %ld).", server_udp_port_number));

											tftp_state = tftp_state_read_from_file;

											block_number = 1;

											if(args.Verbose)
												Printf("Reading block #%ld.\n",block_number);
											
											D(("Reading block #%ld.",block_number));

											SetIoErr(0);

											num_bytes_read = FRead(source_file,tftp_output->th_data,1,SEGSIZE);
											if(num_bytes_read == 0 && IoErr() != 0)
											{
												TEXT error_message[256];

												Fault(IoErr(),NULL,error_message,sizeof(error_message));

												if(!args.Quiet)
													FPrintf(error_output, "%s: Error reading from file \"%s\" (%s).\n","TFTPClient",from_path,error_message);
												
												D(("Error reading from file '%s' (%s).",from_path,error_message));

												send_tftp_error(TFTP_ERROR_UNDEF,"Error reading from file",client_udp_port_number,server_udp_port_number,tftp_packet);

												result = RETURN_ERROR;
												goto out;
											}

											total_num_bytes_transferred += num_bytes_read;

											/* Did we just read the last data to be transmitted? */
											if(num_bytes_read < SEGSIZE)
											{
												last_block_transmitted = TRUE;

												if(args.Verbose)
													Printf("This is the last block to be read.\n");
												
												D(("This is the last block to be read."));
											}

											tftp_output->th_opcode	= TFTP_PACKET_DATA;
											tftp_output->th_block	= block_number;

											tftp_output_length = offsetof(struct tftphdr, th_data) + num_bytes_read;
											tftp_payload_length = num_bytes_read;

											if(args.Verbose)
												Printf("Sending block #%ld (%ld bytes).\n",block_number,tftp_payload_length);
											
											D(("Sending block #%ld (%ld bytes).",block_number,tftp_payload_length));

											send_udp(client_udp_port_number,server_udp_port_number,tftp_output,tftp_output_length);

											/* Restart the timer; make sure that we won't try to
											 * service the returning time request or there will
											 * be trouble.
											 */
											signals_received &= ~time_signal_mask;

											D(("starting the timer"));

											start_time(1);
										}
										else
										{
											if(args.Verbose)
												Printf("Ignoring receipt of acknowledgement for block #%ld.\n",tftp->th_block);
											
											D(("Ignoring receipt of acknowledgement for block #%ld.",tftp->th_block));
										}
									}
									/* Could this be the response to the block we just sent to the server? */
									else if (tftp_state == tftp_state_read_from_file)
									{
										/* Is this really the acknowledgement for the block just sent? */
										if(tftp->th_block == block_number)
										{
											LONG num_bytes_read;

											/* Are we finished now? */
											if(last_block_transmitted)
											{
												if(args.Verbose)
													Printf("Transmission completed.\n");
												
												D(("Transmission completed."));

												break;
											}

											block_number++;

											if(args.Verbose)
											{
												Printf("Server has acknowledged receipt of block #%ld.\n", block_number-1);
												Printf("Reading block #%ld.\n",block_number);
											}

											D(("Server has acknowledged receipt of block #%ld.", block_number-1));
											D(("Reading block #%ld.",block_number));

											SetIoErr(0);

											num_bytes_read = FRead(source_file,tftp_output->th_data,1,SEGSIZE);
											if(num_bytes_read == 0 && IoErr() != 0)
											{
												TEXT error_message[256];

												Fault(IoErr(),NULL,error_message,sizeof(error_message));

												if(!args.Quiet)
													FPrintf(error_output, "%s: Error reading from file \"%s\" (%s).\n","TFTPClient",from_path,error_message);
												
												D(("Error reading from file '%s' (%s).",from_path,error_message));

												send_tftp_error(TFTP_ERROR_UNDEF,"Error reading from file",client_udp_port_number,server_udp_port_number,tftp_packet);

												result = RETURN_ERROR;
												goto out;
											}

											total_num_bytes_transferred += num_bytes_read;

											/* Did we just read the last data to be transmitted?
											 * We also check for block number overflows, which
											 * limits the number of blocks we can safely transmit.
											 */
											if(num_bytes_read < SEGSIZE || ((block_number + 1) & 0xffff) == 0)
											{
												last_block_transmitted = TRUE;

												if(args.Verbose)
													Printf("This is the last block to be read.\n");
												
												D(("This is the last block to be read."));
											}

											tftp_output->th_opcode	= TFTP_PACKET_DATA;
											tftp_output->th_block	= block_number;

											tftp_output_length = offsetof(struct tftphdr, th_data) + num_bytes_read;
											tftp_payload_length = num_bytes_read;

											if(args.Verbose)
												Printf("Sending block #%ld (%ld bytes).\n",block_number,tftp_payload_length);
											
											D(("Sending block #%ld (%ld bytes).",block_number,tftp_payload_length));

											send_udp(client_udp_port_number,server_udp_port_number,tftp_output,tftp_output_length);

											/* Restart the timer; make sure that we won't try to
											 * service the returning time request or there will
											 * be trouble.
											 */
											signals_received &= ~time_signal_mask;

											D(("starting the timer"));

											start_time(1);
										}
										else
										{
											if(args.Verbose)
												Printf("Ignoring receipt of acknowledgement for block #%ld; was expecting block #%ld instead.\n",tftp->th_block,block_number);
											
											D(("Ignoring receipt of acknowledgement for block #%ld; was expecting block #%ld instead.",tftp->th_block,block_number));
										}
									}
									else
									{
										if(args.Verbose)
											Printf("Ignoring receipt of acknowledgement for block #%ld.\n",tftp->th_block);
										
										D(("Ignoring receipt of acknowledgement for block #%ld.",tftp->th_block));
									}
								}
								/* This is an unsupported TFTP operation. We report the problem and then quit. */
								else
								{
									if(!args.Quiet)
										FPrintf(error_output, "%s: Received unsupported TFTP operation %ld -- aborting.\n","TFTPClient",tftp->th_opcode);
									
									D(("Received unsupported TFTP operation %ld -- aborting.",tftp->th_opcode));

									send_tftp_error(TFTP_ERROR_BADOP,"Huh?",client_udp_port_number,server_udp_port_number,tftp_packet);

									result = RETURN_ERROR;
									goto out;
								}
							}
							else
							{
								if(args.Verbose)
								{
									if (checksum != 0)
										Printf("Ignoring UDP datagram with incorrect checksum.\n");
									else if (udp->uh_dport != client_udp_port_number)
										Printf("Ignoring UDP datagram sent to port %ld; expected port %ld.\n", udp->uh_dport, client_udp_port_number);
									else if (server_udp_port_number_known && udp->uh_sport != server_udp_port_number)
										Printf("Ignoring UDP datagram sent by server from port %ld; expected port %ld.\n", udp->uh_sport, server_udp_port_number);
									else
										Printf("Ignoring UDP datagram.\n");
								}

								if (checksum != 0)
									D(("Ignoring UDP datagram with incorrect checksum."));
								else if (udp->uh_dport != client_udp_port_number)
									D(("Ignoring UDP datagram sent to port %ld; expected port %ld.", udp->uh_dport, client_udp_port_number));
								else if (server_udp_port_number_known && udp->uh_sport != server_udp_port_number)
									D(("Ignoring UDP datagram sent by server from port %ld; expected port %ld.", udp->uh_sport, server_udp_port_number));
								else
									D(("Ignoring UDP datagram."));
							}
						}
						/* Is this an ICMP message? Could be a "host unreachable" error. */
						else if (((ip->ip_v_hl >> 4) & 15) == IPVERSION && ip->ip_pr == IPPROTO_ICMP)
						{
							const struct icmp_header * icmp_header = (struct icmp_header *)&ip[1];

							SHOWMSG("datagram contains ICMP data");

							/* The ICMP header and message data checksum should be correct. */
							if(in_cksum(&ip[1],ip->ip_len - sizeof(*ip)) == 0)
							{
								/* We print more detailed information for the
								 * "unreachable" error, but only if we didn't
								 * already finish transmitting data. Some TFTP
								 * servers seem to drop out after the last block
								 * has been transmitted, which produces a
								 * "destination unreachable: bad port" error.
								 */
								if(icmp_header->type == icmp_type_unreach && NOT last_block_transmitted)
								{
									const struct icmp_unreachable_header * unreachable = (struct icmp_unreachable_header *)icmp_header;
									const char * type;
									char number[20];

									switch(unreachable->header.code)
									{
										case icmp_code_unreach_net:

											type = "bad network";
											break;

										case icmp_code_unreach_host:

											type = "bad host";
											break;

										case icmp_code_unreach_protocol:

											type = "bad protocol";
											break;

										case icmp_code_unreach_port:

											type = "bad port";
											break;

										case icmp_code_unreach_needfrag:

											type = "packet dropped due to fragmentation";
											break;

										case icmp_code_unreach_srcfail:

											type = "source route failed";
											break;

										case icmp_code_unreach_net_unknown:

											type = "unknown network";
											break;

										case icmp_code_unreach_host_unknown:

											type = "unknown host";
											break;

										case icmp_code_unreach_isolated:

											type = "source host isolated";
											break;

										case icmp_code_unreach_net_prohib:
										case icmp_code_unreach_host_prohib:

											type = "prohibited access";
											break;

										case icmp_code_unreach_tosnet:

											type = "bad TOS for network";
											break;

										case icmp_code_unreach_toshost:

											type = "bad TOS for host";
											break;

										default:

											sprintf(number,"%d",unreachable->header.code);
											type = number;
											break;
									}

									if(args.Verbose)
										FPrintf(error_output,"%s: Destination unreachable (%s) -- aborting.\n", "TFTPClient", type);
									
									D(("Destination unreachable (%s) -- aborting.", type));

									result = RETURN_ERROR;
									goto out;
								}
								else
								{
									if(args.Verbose)
										Printf("Ignoring ICMP datagram with code=%ld and type=%ld.\n", icmp_header->type, icmp_header->code);
									
									D(("Ignoring ICMP datagram with code=%ld and type=%ld.", icmp_header->type, icmp_header->code));
								}
							}
							else
							{
								if(args.Verbose)
									Printf("Ignoring ICMP datagram with incorrect checksum.\n");
								
								D(("Ignoring ICMP datagram with incorrect checksum."));
							}
						}
						else
						{
							if(args.Verbose)
							{
								Printf("Ignoring IP datagram (version=%ld, protocol=%ld; expected version=%ld).\n",
									((ip->ip_v_hl >> 4) & 15), ip->ip_pr, IPVERSION);
							}

							D(("Ignoring IP datagram (version=%ld, protocol=%ld; expected version=%ld).",
								((ip->ip_v_hl >> 4) & 15), ip->ip_pr, IPVERSION));
						}
					}
					else
					{
						if(args.Verbose)
							Printf("Ignoring IP datagram with incorrect checksum.\n");
						
						D(("Ignoring IP datagram with incorrect checksum."));
					}
				}
				/* Is this an ARP packet? */
				else if (read_request->nior_Type == ETHERTYPE_ARP)
				{
					const struct ARPHeaderEthernet * ahe = read_request->nior_Buffer;

					SHOWMSG("received an ARP packet");

					/* This should be an ARP packet for IPv4 addresses, with
					 * matching hardware (6 bytes) and protocol (4 bytes)
					 * addresses.
					 */
					if(read_request->nior_IOS2.ios2_DataLength >= sizeof(*ahe) &&
					   ahe->ahe_HardwareAddressFormat == ARPHRD_ETHER &&
					   ahe->ahe_ProtocolAddressFormat == ETHERTYPE_IP &&
					   ahe->ahe_HardwareAddressLength == 6 &&
					   ahe->ahe_ProtocolAddressLength == 4)
					{
						/* Is this a request to report the hardware address corresponding
						 * to this tftp client's IPv4 address?
						 */
						if (ahe->ahe_Operation == ARPOP_REQUEST)
						{
							if(ahe->ahe_SenderProtocolAddress != local_ipv4_address && ahe->ahe_TargetProtocolAddress == local_ipv4_address)
							{
								if(args.Verbose)
									Printf("Responding to ARP request.\n");
								
								D(("Responding to ARP request."));

								send_arp_response(ahe->ahe_SenderProtocolAddress,ahe->ahe_SenderHardwareAddress);
							}
							else
							{
								if(args.Verbose)
									Printf("Ignoring ARP request; it's not for us.\n");
								
								D(("Ignoring ARP request; it's not for us."));
							}
						}
						/* Is this a response to this client's query to provide the hardware
						 * address corresponding to the server's IPv4 address?
						 */
						else if (ahe->ahe_Operation == ARPOP_REPLY)
						{
							if(ahe->ahe_SenderProtocolAddress == remote_ipv4_address)
							{
								if(args.Verbose)
									Printf("Received ARP response.\n");
								
								D(("Received ARP response."));

								/* Update the remote TFTP server Ethernet MAC address. */
								memmove(remote_ethernet_address,ahe->ahe_SenderHardwareAddress,sizeof(remote_ethernet_address));

								/* If we are still waiting for the Ethernet MAC address of
								 * the remote server to become available, begin the TFTP
								 * exchange.
								 */
								if(tftp_state == tftp_state_request_ethernet_address)
								{
									if(args.Verbose)
										Printf("Trying to begin transmission of file \"%s\".\n", local_filename);
									
									D(("Trying to begin transmission of file '%s'.", local_filename));
									
									tftp_state = (from_ipv4_address == 0) ? tftp_state_request_write : tftp_state_request_read;

									start_tftp(tftp_state == tftp_state_request_write ? TFTP_PACKET_WRQ : TFTP_PACKET_RRQ,
										remote_filename,client_udp_port_number,server_udp_port_number,tftp_packet);

									/* Restart the timer; make sure that we won't try to
									 * service the returning time request or there will
									 * be trouble.
									 */
									signals_received &= ~time_signal_mask;

									D(("starting the timer"));

									start_time(1);
								}
							}
							else
							{
								if(args.Verbose)
									Printf("Ignoring ARP response; it's not for us.\n");
								
								D(("Ignoring ARP response; it's not for us."));
							}
						}
						else
						{
							if(args.Verbose)
								Printf("Ignoring ARP packet with unsupported operation %ld.\n", ahe->ahe_Operation);
							
							D(("Ignoring ARP packet with unsupported operation %ld.", ahe->ahe_Operation));
						}
					}
					else
					{
						if(args.Verbose)
							Printf("Ignoring ARP packet.\n");
						
						D(("Ignoring ARP packet."));
					}
				}

				/* Put the read request back into circulation. */
				D(("restarting read request 0x%08lx", read_request));
				send_net_io_read_request(read_request,read_request->nior_Type);
			}
			else
			{
				/* Wait for further I/O requests to come in. */
				signals_received &= ~net_signal_mask;
			}
		}

		/* A timeout has elapsed? */
		if(signals_received & time_signal_mask)
		{
			if(time_in_use)
				WaitIO((struct IORequest *)time_request);
				
			time_in_use = FALSE;

			/* No response to the ARP request has arrived yet? */
			if (tftp_state == tftp_state_request_ethernet_address)
			{
				if(num_arp_resolution_attempts == 0)
				{
					if(args.Verbose)
						FPrintf(error_output, "%s: No response to ARP query received -- aborting.\n","TFTPClient");
					
					D(("No response to ARP query received -- aborting."));

					result = RETURN_ERROR;
					goto out;
				}

				if(args.Verbose)
					Printf("Repeating ARP query...\n");
				
				D(("Repeating ARP query."));

				broadcast_arp_query(remote_ipv4_address);

				num_arp_resolution_attempts--;

				D(("starting the timer"));

				start_time(1);
			}
			/* The server has not replied to our write/read request yet? */
			else if (tftp_state == tftp_state_request_write || tftp_state == tftp_state_request_read)
			{
				if(args.Verbose)
					Printf("Trying to begin transmission of file \"%s\" again.\n", local_filename);
				
				D(("Trying to begin transmission of file '%s' again.", local_filename));
				
				start_tftp(tftp_state == tftp_state_request_write ? TFTP_PACKET_WRQ : TFTP_PACKET_RRQ,
					remote_filename,client_udp_port_number,server_udp_port_number,tftp_packet);

				D(("starting the timer"));

				start_time(1);
			}
			/* The server has not yet acknowledged the receipt of the block we sent to it? */
			else if (tftp_state == tftp_state_read_from_file)
			{
				if(args.Verbose)
					Printf("Sending block #%ld again (%ld bytes).\n",block_number,tftp_payload_length);
				
				D(("Sending block #%ld again (%ld bytes).",block_number,tftp_payload_length));

				send_udp(client_udp_port_number,server_udp_port_number,tftp_output,tftp_output_length);

				D(("starting the timer"));

				start_time(1);
			}
			/* The server has not yet sent the next block to write? */
			else if (tftp_state == tftp_state_write_to_file)
			{
				if(last_block_transmitted)
				{
					num_eof_acknowledgements--;
					if(num_eof_acknowledgements == 0)
					{
						if(args.Verbose)
							Printf("Transmission completed.\n");
						
						D(("Transmission completed."));

						break;
					}
				}

				if(args.Verbose)
					Printf("Acknowledging receipt of block #%ld again.\n",block_number-1);
				
				D(("Acknowledging receipt of block #%ld again.",block_number-1));

				send_tftp_acknowledgement(block_number-1,client_udp_port_number,server_udp_port_number,tftp_packet);

				D(("starting the timer"));

				start_time(1);
			}

			signals_received &= ~time_signal_mask;
		}
	}

	result = RETURN_OK;

 out:

	if(args.Verbose)
		Printf("A total of %ld bytes were transmitted.\n",total_num_bytes_transferred);
	
	D(("A total of %ld bytes were transmitted.",total_num_bytes_transferred));

	cleanup();

	if(rda != NULL)
		FreeArgs(rda);

	if(source_file != (BPTR)NULL)
		Close(source_file);

	/* If a file was created to stored the received data
	 * in, close it and perform some postprocessing
	 * on it.
	 */
	if(destination_file != (BPTR)NULL)
	{
		Close(destination_file);

		if(to_path != NULL)
		{
			/* Delete an incomplete file. */
			if(delete_destination_file)
				DeleteFile(to_path);
			/* Keep the file, but mark it as not executable,
			 * just to be safe.
			 */
			else
				SetProtection(to_path, FIBF_EXECUTE);
		}
	}

	return(result);
}
