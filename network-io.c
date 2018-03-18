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

#include <exec/memory.h>

#include <string.h>
#include <stdio.h>

/****************************************************************************/

#define __USE_INLINE__
#include <proto/exec.h>
#include <proto/dos.h>

/****************************************************************************/

#include <clib/alib_protos.h>

/****************************************************************************/

#include "network-ip-udp.h"
#include "network-tftp.h"
#include "error-codes.h"
#include "network-io.h"
#include "testing.h"
#include "args.h"

/****************************************************************************/

#include "macros.h"
#include "compiler.h"
#include "assert.h"

/****************************************************************************/

/* This keeps track of all net I/O requests allocated. */
static struct List net_io_list;
BOOL net_io_list_initialized;

/* Network read and write operations use different message ports. */
static struct MsgPort * net_control_port;
struct MsgPort * net_read_port;

/****************************************************************************/

/* The network driver is opened with the control request, and individual
 * write operations are performed only with the write request.
 */
static struct NetIORequest * control_request;
struct NetIORequest * write_request;

/****************************************************************************/

/* This data is used by the ARP requests. */
UBYTE local_ethernet_address[SANA2_MAX_ADDR_BYTES];
ULONG local_ipv4_address;

UBYTE remote_ethernet_address[SANA2_MAX_ADDR_BYTES];
ULONG remote_ipv4_address;

/****************************************************************************/

/* Start a SANA-II read request command, to be picked up later when
 * it completes execution.
 */
void
send_net_io_read_request(struct NetIORequest * nior,UWORD type)
{
	ENTER();

	SHOWPOINTER(nior);
	SHOWVALUE(type);

	ASSERT( nior != NULL );
	ASSERT( nior->nior_IOS2.ios2_Req.io_Device != NULL );
	ASSERT( nior->nior_IOS2.ios2_Req.io_Message.mn_ReplyPort != NULL );
	ASSERT( NOT nior->nior_InUse );
	ASSERT( nior->nior_Link.mln_Succ != NULL && nior->nior_Link.mln_Pred != NULL );
	
	nior->nior_Type						= type;
	nior->nior_IOS2.ios2_PacketType		= nior->nior_Type;
	nior->nior_IOS2.ios2_Req.io_Command	= CMD_READ;
	nior->nior_IOS2.ios2_Data			= nior;
	nior->nior_InUse					= TRUE;

	SendIO((struct IORequest *)nior);

	LEAVE();
}

/****************************************************************************/

/* Find the SANA-II IORequest which corresponds to the bookkeeping
 * data structure which keeps track of all the network I/O requests
 * allocated by this program.
 */
static struct NetIORequest *
link_to_net_request(const struct MinNode * mn)
{
	struct NetIORequest * result;

	ASSERT( mn != NULL );

	/* NOTE: link must be located right behind the IOSana2Req. */
	result = (struct NetIORequest *)( ((struct IOSana2Req *)mn)-1 );

	ASSERT( result->nior_Link.mln_Succ != NULL && result->nior_Link.mln_Pred != NULL );

	return(result);
}

/****************************************************************************/

/* Abort a network I/O request currently being processed.
 * Once this function returns the I/O request is ready
 * to be reused.
 */
static void
abort_net_request(struct NetIORequest * nior)
{
	ENTER();

	SHOWPOINTER( nior );
	SHOWVALUE( nior->nior_InUse  );

	/* Is this IORequest valid and still in play? */
	if(nior != NULL && nior->nior_InUse)
	{
		ASSERT( nior->nior_IOS2.ios2_Req.io_Device != NULL );
		ASSERT( nior->nior_Link.mln_Succ != NULL && nior->nior_Link.mln_Pred != NULL );

		/* Abort the IORequest operation if it has not finished yet. */
		if(CheckIO((struct IORequest *)nior) == BUSY)
		{
			SHOWMSG("aborting pending I/O request");

			AbortIO((struct IORequest *)nior);
		}

		SHOWMSG("waiting for I/O request to return");

		WaitIO((struct IORequest *)nior);

		nior->nior_InUse = FALSE;
	}

	LEAVE();
}

/* Free the resources allocated for a network I/O request, making
 * sure that it is not currently busy.
 */
static void
delete_net_request(struct NetIORequest * nior)
{
	ENTER();

	SHOWPOINTER(nior);

	if(nior != NULL)
	{
		abort_net_request(nior);

		ASSERT( nior->nior_Link.mln_Succ != NULL && nior->nior_Link.mln_Pred != NULL );

		Remove((struct Node *)&nior->nior_Link);
		nior->nior_Link.mln_Succ = nior->nior_Link.mln_Pred = NULL;

		if(nior->nior_IOS2.ios2_Req.io_Device != NULL)
		{
			if(NOT nior->nior_IsDuplicate)
			{
				SHOWMSG("closing device driver");

				ASSERT( NOT nior->nior_InUse );

				CloseDevice((struct IORequest *)nior);
			}

			nior->nior_IOS2.ios2_Req.io_Device = NULL;
		}

		if(nior->nior_Buffer != NULL)
		{
			FreeVec(nior->nior_Buffer);
			nior->nior_Buffer = NULL;
		}

		FreeVec(nior);
	}

	LEAVE();
}

/* Make a copy of a network I/O request, substituting the reply port if needed,
 * and allocating a new data buffer if desired.
 */
static struct NetIORequest *
duplicate_net_request(const struct NetIORequest * orig, struct MsgPort * reply_port, ULONG buffer_size)
{
	struct NetIORequest * result = NULL;
	struct NetIORequest * nior;

	ASSERT( orig != NULL );
	ASSERT( orig->nior_IOS2.ios2_Req.io_Device != NULL );
	ASSERT( orig->nior_Link.mln_Succ != NULL && orig->nior_Link.mln_Pred != NULL );

	nior = (struct NetIORequest *)AllocVec(sizeof(*nior), MEMF_ANY|MEMF_PUBLIC|MEMF_CLEAR);
	if(nior == NULL)
		goto out;

	/* Keep track of this network I/O request, making it easier to clean up later. */
	AddTail(&net_io_list,(struct Node *)&nior->nior_Link);

	/* Make a copy of the SANA-II I/O request, and nothing else (structure copy). */
	nior->nior_IOS2 = orig->nior_IOS2;

	/* Make sure that this is safe to use with CheckIO() and WaitIO(). */
	nior->nior_IOS2.ios2_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;

	/* Replace the reply port if needed. */
	if(reply_port != NULL)
		nior->nior_IOS2.ios2_Req.io_Message.mn_ReplyPort = reply_port;

	/* Remember not to close the device driver when this request is freed. */
	nior->nior_IsDuplicate = TRUE;

	nior->nior_BufferSize = buffer_size;

	nior->nior_Buffer = AllocVec(nior->nior_BufferSize, MEMF_ANY|MEMF_PUBLIC);
	if(nior->nior_Buffer == NULL)
		goto out;

	result = nior;
	nior = NULL;

 out:

	if(nior != NULL)
		delete_net_request(nior);

	return(result);
}

/* Allocate a network I/O request, using a specific reply port (mandatory).
 */
static struct NetIORequest *
create_net_request(struct MsgPort * reply_port)
{
	struct NetIORequest * result = NULL;
	struct NetIORequest * nior;

	nior = (struct NetIORequest *)AllocVec(sizeof(*nior), MEMF_ANY|MEMF_PUBLIC|MEMF_CLEAR);
	if(nior == NULL)
		goto out;

	/* Keep track of this network I/O request, making it easier to clean up later. */
	AddTail(&net_io_list,(struct Node *)&nior->nior_Link);

	nior->nior_IOS2.ios2_Req.io_Message.mn_Node.ln_Type	= NT_REPLYMSG;
	nior->nior_IOS2.ios2_Req.io_Message.mn_Length		= sizeof(nior->nior_IOS2);
	nior->nior_IOS2.ios2_Req.io_Message.mn_ReplyPort	= reply_port;

	result = nior;
	nior = NULL;

 out:

	if(nior != NULL)
		delete_net_request(nior);

	return(result);
}

/****************************************************************************/

/* OpenDevice() will only open device drivers which are either currently
 * already in memory, or which can be found in "DEVS:", but SANA-II network
 * device drivers are typically found in "DEVS:Networks". This function
 * will do its best to open a network driver even if it's found in
 * "DEVS:Networks".
 */
static LONG
NiceOpenDevice(STRPTR name,LONG unit,struct IORequest *ior,LONG flags)
{
	LONG error;

	error = OpenDevice(name,unit,ior,flags);
	if(error == IOERR_OPENFAIL && FilePart(name) == name)
	{
		const char prefix[] = "Networks/";
		STRPTR new_name;

		new_name = AllocVec(strlen(prefix) + strlen(name) + 1, MEMF_ANY);
		if(new_name != NULL)
		{
			strcpy(new_name,prefix);
			strcat(new_name,name);

			error = OpenDevice(new_name,unit,ior,flags);

			FreeVec(new_name);
		}
	}

	return(error);
}

/****************************************************************************/

/* Copy from the network device transmit buffer to the network I/O
 * request buffer. This is what the CMD_WRITE, S2_MULTICAST and S2_BROADCAST
 * commands use.
 *
 * The buffer referred to in the "sana2_byte_copy_from_buff" function name
 * is the client's buffer. Hence, this function copies from the client's
 * buffer to the network driver's buffer so that the driver may transmit the
 * data.
 */
static LONG ASM SAVE_DS
sana2_byte_copy_from_buff(
	REG(a0,UBYTE *						to),
	REG(a1,const struct NetIORequest *	from),
	REG(d0,ULONG						n))
{
	LONG result;

	ENTER();

	SHOWPOINTER(to);
	SHOWPOINTER((APTR)from);
	SHOWVALUE(n);

	ASSERT( to != NULL || n == 0 );
	ASSERT( n <= from->nior_BufferSize );
	ASSERT( from->nior_IOS2.ios2_Req.io_Device != NULL );

	/* Do not copy more data than the buffer will hold. */
	if(n <= from->nior_BufferSize && from->nior_Buffer != NULL)
	{
		memmove(to,from->nior_Buffer,n);

		result = TRUE;
	}
	/* We have a buffer overflow: flag this as an error. */
	else
	{
		result = FALSE;
	}

	RETURN(result);
	return(result);
}

/* Copy to the network device transmit buffer from the network I/O
 * request buffer. This is what the CMD_READ and S2_READORPHAN
 * commands use.
 *
 * The buffer referred to in the "sana2_byte_copy_to_buff" function name
 * is the client's buffer. Hence, this function copies data received
 * by the network driver to the client's buffer.
 */
static LONG ASM SAVE_DS
sana2_byte_copy_to_buff(
	REG(a0,struct NetIORequest *	to),
	REG(a1,const UBYTE *			from),
	REG(d0,ULONG					n))
{
	LONG result;

	ENTER();

	SHOWPOINTER(to);
	SHOWPOINTER((APTR)from);
	SHOWVALUE(n);

	ASSERT( from != NULL || n == 0 );
	ASSERT( n <= to->nior_BufferSize );
	ASSERT( to->nior_IOS2.ios2_Req.io_Device != NULL );

	/* Do not copy more data than the buffer will hold. */
	if(n <= to->nior_BufferSize && to->nior_Buffer != NULL)
	{
		memmove(to->nior_Buffer,from,n);

		result = TRUE;
	}
	/* We have a buffer overflow: flag this as an error. */
	else
	{
		result = FALSE;
	}

	RETURN(result);
	return(result);
}

/****************************************************************************/

/* Attempt to return the client's buffer address so that the network driver
 * may transmit its contents. This is what the CMD_WRITE, S2_MULTICAST and
 * S2_BROADCAST commands may use.
 *
 * Returns the address of the client's buffer if it matches the alignment
 * requirements; NULL otherwise.
 */
static APTR ASM SAVE_DS
sana2_dma_copy_from_buff32(REG(a0,const struct NetIORequest * from))
{
	APTR result = NULL;

	ENTER();

	SHOWPOINTER((APTR)from);

	ASSERT( from != NULL );
	ASSERT( from->nior_IOS2.ios2_Req.io_Device != NULL );

	/* The buffer address must be a multiple of 4 (32 bit
	 * alignment). We bail if this is not case, or if the
	 * buffer address is invalid.
	 */
	if((((ULONG)from->nior_Buffer) % 4) != 0 || from->nior_Buffer == NULL)
		goto out;

	result = from->nior_Buffer;

 out:

	RETURN(result);
	return(result);
}

/* Attempt to return the client's buffer address so that the network
 * driver may fill it with data being received. This is what the CMD_READ
 * and S2_READORPHAN commands may use.
 *
 * Returns the address of the client's buffer if it matches the alignment
 * requirements and if the size of the buffer is large enough to hold
 * the number of data bytes which the network driver may want to store
 * there; NULL otherwise.
 */
static APTR ASM SAVE_DS
sana2_dma_copy_to_buff32(REG(a0,const struct NetIORequest * to))
{
	ULONG n, remaining_bytes;
	APTR result = NULL;

	ENTER();

	SHOWPOINTER((APTR)to);

	ASSERT( to != NULL );
	ASSERT( to->nior_IOS2.ios2_Req.io_Device != NULL );

	/* How much data the driver may want to store in the buffer is
	 * given in to->nior_IOS2.ios2_DataLength. We must round this
	 * up to a multiple of 4 (32 bit alignment) to make sure that
	 * we have that much space in the buffer.
	 */
	n = to->nior_IOS2.ios2_DataLength;

	remaining_bytes = 4 - (n % 4);
	if(remaining_bytes < 4)
	{
		/* Paranoia: avoid an overflow. */
		if(n + remaining_bytes > n)
			n += remaining_bytes;
	}

	/* The client buffer must be large enough to store
	 * as many bytes as the driver may want to. We
	 * bail out if this is not the case.
	 */
	if(n > to->nior_BufferSize)
		goto out;

	/* The buffer address must be a multiple of 4 (32 bit
	 * alignment). We bail if this is not case, or if the
	 * buffer address is invalid.
	 */
	if((((ULONG)to->nior_Buffer) % 4) != 0 || to->nior_Buffer == NULL)
		goto out;

	result = to->nior_Buffer;

 out:

	RETURN(result);
	return(result);
}

/****************************************************************************/

/* This function stops all I/O operations and releases all the resources
 * allocated by the network_setup() function.
 */
void
network_cleanup(void)
{
	ENTER();

	if(net_io_list_initialized)
	{
		struct NetIORequest * nior;
		struct MinNode * mn;
		struct MinNode * mn_next;

		/* Try to abort all pending I/O requests, and for good measure
		 * also render the copying functions useless.
		 */
		for(mn = (struct MinNode *)net_io_list.lh_Head ;
			mn->mln_Succ != NULL ;
			mn = mn->mln_Succ)
		{
			nior = link_to_net_request(mn);

			if(nior->nior_InUse)
			{
				/* Abort the IORequest operation if it has not finished yet. */
				if(CheckIO((struct IORequest *)nior) == BUSY)
				{
					D(("aborting pending I/O request 0x%08lx", nior));

					AbortIO((struct IORequest *)nior);
				}
			}

			nior->nior_BufferSize = 0;
		}

		SHOWMSG("stopping all pending I/O requests");

		/* Stop all network I/O requests currently in play. */
		for(mn = (struct MinNode *)net_io_list.lh_Head ;
			mn->mln_Succ != NULL ;
			mn = mn->mln_Succ)
		{
			nior = link_to_net_request(mn);

			if(nior->nior_InUse)
			{
				D(("waiting for pending I/O request 0x%08lx to return...", nior));

				WaitIO((struct IORequest *)nior);

				nior->nior_InUse = FALSE;
			}
		}

		SHOWMSG("releasing all allocated memory for I/O request copies");

		/* Release all network I/O requests except for the
		 * one which was used to open the network device
		 * driver.
		 */
		for(mn = (struct MinNode *)net_io_list.lh_Head ;
			mn->mln_Succ != NULL ;
			mn = mn_next)
		{
			mn_next = mn->mln_Succ;

			nior = link_to_net_request(mn);

			if(nior->nior_IsDuplicate)
				delete_net_request(nior);
		}
	}

	if(control_request != NULL)
	{
		delete_net_request(control_request);
		control_request = NULL;
	}

	if(net_read_port != NULL)
	{
		DeleteMsgPort(net_read_port);
		net_read_port = NULL;
	}

	if(net_control_port != NULL)
	{
		DeleteMsgPort(net_control_port);
		net_control_port = NULL;
	}

	LEAVE();
}

/****************************************************************************/

/* Some network device drivers may prefer to call this hook function rather
 * to call the buffer management functions provided at OpenDevice() time.
 */
static LONG ASM SAVE_DS
sana2_hook_function(
	REG(a0,struct Hook *				hook),
	REG(a2,struct NetIORequest *		sana2req),
	REG(a1,struct SANA2CopyHookMsg *	schm))
{
	LONG result;

	/* Which operation should be performed? We just look up what is
	 * called for, then directly call the corresponding "traditional"
	 * SANA-II buffer management function.
	 */
	switch(schm->schm_Method)
	{
		case S2_CopyToBuff:

			result = sana2_byte_copy_to_buff(sana2req,schm->schm_From,schm->schm_Size);
			break;

		case S2_CopyFromBuff:

			result = sana2_byte_copy_from_buff(schm->schm_To,sana2req,schm->schm_Size);
			break;

		case S2_DMACopyToBuff32:

			result = (LONG)sana2_dma_copy_to_buff32(sana2req);
			break;

		case S2_DMACopyFromBuff32:

			result = (LONG)sana2_dma_copy_from_buff32(sana2req);
			break;

		default:

			result = 0;
			break;
	}

	return(result);
}

/****************************************************************************/

/* Initialize the network I/O operations, allocating SANA-II I/O requests,
 * opening the network device driver, etc. Returns 0 on success,
 * and -1 on failure.
 */
int
network_setup(BPTR error_output, const struct cmd_args * args)
{
	static struct TagItem buffer_management[] =
	{
		{ S2_CopyFromBuff,		(ULONG)sana2_byte_copy_from_buff },
		{ S2_CopyToBuff,		(ULONG)sana2_byte_copy_to_buff },
		{ S2_DMACopyFromBuff32,	(ULONG)sana2_dma_copy_from_buff32 },
		{ S2_DMACopyToBuff32,	(ULONG)sana2_dma_copy_to_buff32 },

		{ TAG_END, 0 }
	};

	static Tag hook_capabilities[] =
	{
		S2_CopyToBuff,
		S2_CopyFromBuff,
		S2_DMACopyToBuff32,
		S2_DMACopyFromBuff32,

		TAG_END
	};

	static struct Sana2Hook sana2_hook;

	struct Sana2DeviceQuery s2dq;
	const char * error_text;
	char other_error_text[100];
	const char * wire_error_text;
	char other_wire_error_text[100];
	UBYTE default_ethernet_address[SANA2_MAX_ADDR_BYTES];
	int result = FAILURE;
	struct NetIORequest * read_request;
	ULONG buffer_size = 1500;
	LONG error;
	int i;

	NewList(&net_io_list);
	net_io_list_initialized = TRUE;

	/* If the testing functionality is compiled into this command, the threshold
	 * values for the various test operations will be read from environment
	 * variables.
	 */
	#if defined(TESTING)
	{
		TEXT value[16];
		LONG number;

		/* Percentage of packets which deliver received data which should be discarded. */
		if(GetVar("DROPRX",value,sizeof(value),0) > 0)
		{
			if(StrToLong(value,&number) > 0 && 0 <= number && number <= 100)
				drop_rx = number;
		}

		/* Percentage of packets to be discarded instead of sending them. */
		if(GetVar("DROPTX",value,sizeof(value),0) > 0)
		{
			if(StrToLong(value,&number) > 0 && 0 <= number && number <= 100)
				drop_tx = number;
		}

		/* Percentage of packets which deliver received data whose contents should be corrupted. */
		if(GetVar("TRASHRX",value,sizeof(value),0) > 0)
		{
			if(StrToLong(value,&number) > 0 && 0 <= number && number <= 100)
				trash_rx = number;
		}

		/* Percentage of packets to be sent whose contents should be corrupted. */
		if(GetVar("TRASHTX",value,sizeof(value),0) > 0)
		{
			if(StrToLong(value,&number) > 0 && 0 <= number && number <= 100)
				trash_tx = number;
		}

		srand(time(NULL));
	}
	#endif /* TESTING */

	/* Prepare for opening the network device driver. */
	net_read_port = CreateMsgPort();
	if(net_read_port == NULL)
	{
		if(!args->Quiet)
			FPrintf(error_output, "%s: Could not create network read MsgPort.\n","TFTPClient");

		goto out;
	}

	net_control_port = CreateMsgPort();
	if(net_control_port == NULL)
	{
		if(!args->Quiet)
			FPrintf(error_output, "%s: Could not create network write MsgPort.\n","TFTPClient");

		goto out;
	}

	control_request = create_net_request(net_control_port);
	if(control_request == NULL)
	{
		if(!args->Quiet)
			PrintFault(ERROR_NO_FREE_STORE,"TFTPClient");

		goto out;
	}

	SHOWPOINTER(control_request);

	control_request->nior_IOS2.ios2_BufferManagement = buffer_management;

	error = NiceOpenDevice(args->DeviceName,(*args->DeviceUnit),(struct IORequest *)control_request, 0);
	if(error != OK)
	{
		if(!args->Quiet)
		{
			error_text = get_io_error_text(error);
			if(error_text == NULL)
			{
				sprintf(other_error_text,"error=%d",error);
				error_text = other_error_text;
			}

			FPrintf(error_output, "%s: Could not open '%s', unit %ld (%s).\n","TFTPClient", args->DeviceName,(*args->DeviceUnit), error_text);
		}

		goto out;
	}

	/* The network driver may prefer to call a hook function instead
	 * of using the supplied buffer management functions. We merely
	 * provide the hook as a convenience and do not require it to be
	 * used. The network driver should have refused to open if it
	 * did not support the "traditional" SANA-II buffer management
	 * functions.
	 */
	sana2_hook.s2h_Hook.h_Entry	= (HOOKFUNC)sana2_hook_function;
	sana2_hook.s2h_Methods		= hook_capabilities;

	control_request->nior_IOS2.ios2_Req.io_Command	= S2_SANA2HOOK;
	control_request->nior_IOS2.ios2_Data			= &sana2_hook;
	control_request->nior_IOS2.ios2_DataLength		= sizeof(sana2_hook);

	DoIO((struct IORequest *)control_request);

	/* Find out which type of networking hardware the driver is responsible
	 * for, and how large the transmission buffer may be.
	 */
	control_request->nior_IOS2.ios2_Req.io_Command	= S2_DEVICEQUERY;
	control_request->nior_IOS2.ios2_StatData		= &s2dq;
	control_request->nior_IOS2.ios2_WireError		= 0;

	memset(&s2dq,0,sizeof(s2dq));

	/* The SizeAvailable field must not suggest a larger value than
	 * the a2065.device is prepared to accept, or strange things
	 * may happen. We are not interested in the RawMTU field here,
	 * so this restriction is harmless.
	 */
	s2dq.SizeAvailable = sizeof(s2dq) - sizeof(s2dq.RawMTU);

	ASSERT( NOT control_request->nior_InUse );

	error = DoIO((struct IORequest *)control_request);
	if(error != OK)
	{
		if(!args->Quiet)
		{
			error_text = get_io_error_text(error);
			if(error_text == NULL)
			{
				sprintf(other_error_text,"error=%d",error);
				error_text = other_error_text;
			}

			if(control_request->nior_IOS2.ios2_WireError == 0)
			{
				wire_error_text = "no wire error";
			}
			else
			{
				wire_error_text = get_wire_error_text(control_request->nior_IOS2.ios2_WireError);
				if(wire_error_text == NULL)
				{
					sprintf(other_wire_error_text,"wire error=%d",control_request->nior_IOS2.ios2_WireError);
					wire_error_text = other_wire_error_text;
				}
			}

			FPrintf(error_output, "%s: Could not query '%s', unit %ld parameters (%s, %s).\n","TFTPClient",
				args->DeviceName,(*args->DeviceUnit), error_text, wire_error_text);
		}
		
		goto out;
	}

	/* This should be an Ethernet device with the standard address size (6 bytes). */
	if(s2dq.HardwareType != S2WireType_Ethernet || s2dq.AddrFieldSize != 48)
	{
		if(!args->Quiet)
			FPrintf(error_output, "%s: '%s', unit %ld does not correspond to an Ethernet device.\n","TFTPClient", args->DeviceName,(*args->DeviceUnit));

		goto out;
	}

	/* The transmission buffer size should be as large as the device supports, and not smaller. */
	if (s2dq.MTU > 0 && buffer_size != s2dq.MTU)
		buffer_size = s2dq.MTU;

	/* For TFTP with 512 byte segments the minimum Ethernet packet size
	 * supported should be no lower than about 540 bytes.
	 */
	if(buffer_size < sizeof(struct ip) + sizeof(struct udphdr) + SEGSIZE)
	{
		if(!args->Quiet)
		{
			FPrintf(error_output, "%s: Maximum transmission unit size (%ld bytes) of '%s', unit %ld is too small.\n","TFTPClient", buffer_size,
				args->DeviceName,(*args->DeviceUnit));
		}

		goto out;
	}

	/* Find out which Ethernet address this device currently uses. */
	control_request->nior_IOS2.ios2_Req.io_Command	= S2_GETSTATIONADDRESS;
	control_request->nior_IOS2.ios2_StatData		= NULL;
	control_request->nior_IOS2.ios2_WireError		= 0;

	ASSERT( NOT control_request->nior_InUse );

	error = DoIO((struct IORequest *)control_request);
	if(error != OK)
	{
		if(!args->Quiet)
		{
			error_text = get_io_error_text(error);
			if(error_text == NULL)
			{
				sprintf(other_error_text,"error=%d",error);
				error_text = other_error_text;
			}

			if(control_request->nior_IOS2.ios2_WireError == 0)
			{
				wire_error_text = "no wire error";
			}
			else
			{
				wire_error_text = get_wire_error_text(control_request->nior_IOS2.ios2_WireError);
				if(wire_error_text == NULL)
				{
					sprintf(other_wire_error_text,"wire error=%d",control_request->nior_IOS2.ios2_WireError);
					wire_error_text = other_wire_error_text;
				}
			}

			FPrintf(error_output, "%s: Could not obtain '%s', unit %ld station address (%s, %s).\n","TFTPClient",
				args->DeviceName,(*args->DeviceUnit), error_text, wire_error_text);
		}
		
		goto out;
	}

	memmove(default_ethernet_address,control_request->nior_IOS2.ios2_DstAddr,sizeof(default_ethernet_address));
	memmove(local_ethernet_address,control_request->nior_IOS2.ios2_SrcAddr,sizeof(local_ethernet_address));

	/* Set the device's Ethernet address to the default, unless it has
	 * already been taken care of.
	 */
	control_request->nior_IOS2.ios2_Req.io_Command	= S2_CONFIGINTERFACE;
	control_request->nior_IOS2.ios2_WireError		= 0;

	memmove(control_request->nior_IOS2.ios2_SrcAddr, control_request->nior_IOS2.ios2_DstAddr, SANA2_MAX_ADDR_BYTES);

	ASSERT( NOT control_request->nior_InUse );

	error = DoIO((struct IORequest *)control_request);
	if(error != OK && control_request->nior_IOS2.ios2_WireError != S2WERR_IS_CONFIGURED)
	{
		if(!args->Quiet)
		{
			error_text = get_io_error_text(error);
			if(error_text == NULL)
			{
				sprintf(other_error_text,"error=%d",error);
				error_text = other_error_text;
			}

			if(control_request->nior_IOS2.ios2_WireError == 0)
			{
				wire_error_text = "no wire error";
			}
			else
			{
				wire_error_text = get_wire_error_text(control_request->nior_IOS2.ios2_WireError);
				if(wire_error_text == NULL)
				{
					sprintf(other_wire_error_text,"wire error=%d",control_request->nior_IOS2.ios2_WireError);
					wire_error_text = other_wire_error_text;
				}
			}

			FPrintf(error_output, "%s: Could not set '%s', unit %ld station address (%s, %s).\n","TFTPClient",
				args->DeviceName,(*args->DeviceUnit), error_text, wire_error_text);
		}
		
		goto out;
	}

	/* If we succeeded in setting the device address to the default address, then
	 * the default address is the current address now.
	 */
	if(error == OK)
		memmove(local_ethernet_address,default_ethernet_address,sizeof(local_ethernet_address));

	write_request = duplicate_net_request(control_request, NULL, buffer_size);
	if(write_request == NULL)
	{
		if(!args->Quiet)
			PrintFault(ERROR_NO_FREE_STORE,"TFTPClient");

		goto out;
	}

	/* We set up four ARP read requests and start them (asynchronously). */
	for(i = 0 ; i < 4 ; i++)
	{
		read_request = duplicate_net_request(control_request, net_read_port, buffer_size);
		if(read_request == NULL)
		{
			if(!args->Quiet)
				PrintFault(ERROR_NO_FREE_STORE,"TFTPClient");

			goto out;
		}

		send_net_io_read_request(read_request,ETHERTYPE_ARP);
	}

	/* We set up eight IP read requests and start them (asynchronously). */
	for(i = 0 ; i < 8 ; i++)
	{
		read_request = duplicate_net_request(control_request, net_read_port, buffer_size);
		if(read_request == NULL)
		{
			if(!args->Quiet)
				PrintFault(ERROR_NO_FREE_STORE,"TFTPClient");

			goto out;
		}

		send_net_io_read_request(read_request,ETHERTYPE_IP);
	}

	result = OK;

 out:

	return(result);
}
