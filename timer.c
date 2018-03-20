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

/****************************************************************************/

#define __USE_INLINE__
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>

/****************************************************************************/

#include "error-codes.h"
#include "timer.h"

/****************************************************************************/

#include "macros.h"
#include "assert.h"

/****************************************************************************/

/* Variables for use with the poll timer, which triggers retransmission
 * of messages and acknowledgements.
 */
struct MsgPort *		time_port;
struct timerequest *	time_request;
BOOL					time_in_use;

/****************************************************************************/

/* Stop the interval timer, if it's currently busy. This function is
 * safe to call even if it is not currently busy, or if the interval timer
 * has never been initialized.
 */
void
stop_time(void)
{
	if(time_in_use)
	{
		ASSERT( time_request != NULL );
		ASSERT( time_request->tr_node.io_Device != NULL );

		if(CheckIO((struct IORequest *)time_request) == BUSY)
			AbortIO((struct IORequest *)time_request);

		WaitIO((struct IORequest *)time_request);

		time_in_use = FALSE;
	}
}

/****************************************************************************/

/* Start the interval timer so that it expires after a given
 * number of seconds. This function is safe to call if the
 * interval timer is currently still ticking.
 */
void
start_time(ULONG seconds)
{
	stop_time();

	ASSERT( NOT time_in_use );

	ASSERT( time_request != NULL );
	ASSERT( time_request->tr_node.io_Device != NULL );

	time_request->tr_node.io_Command	= TR_ADDREQUEST;
	time_request->tr_time.tv_secs		= seconds;
	time_request->tr_time.tv_micro		= 0;

	SetSignal(0, (1UL << time_port->mp_SigBit));

	SendIO((struct IORequest *)time_request);

	time_in_use = TRUE;
}

/****************************************************************************/

/* This initializes the interval timer. */
int
timer_setup(BPTR error_output, const struct cmd_args * args)
{
	const char * error_text;
	char other_error_text[100];
	int result = FAILURE;
	LONG error;

	time_port = CreateMsgPort();
	if(time_port == NULL)
	{
		if(!args->Quiet)
			PrintFault(ERROR_NO_FREE_STORE,"TFTPClient");

		D(("could not create timer message port"));

		goto out;
	}

	time_request = (struct timerequest *)CreateIORequest(time_port, sizeof(*time_request));
	if(time_request == NULL)
	{
		if(!args->Quiet)
			PrintFault(ERROR_NO_FREE_STORE,"TFTPClient");

		D(("could not create timer I/O request"));

		goto out;
	}

	error = OpenDevice(TIMERNAME,UNIT_VBLANK,(struct IORequest *)time_request,0);
	if(error != OK)
	{
		error_text = get_io_error_text(error);
		if(error_text == NULL)
		{
			sprintf(other_error_text,"error=%d",error);
			error_text = other_error_text;
		}

		if(!args->Quiet)
			FPrintf(error_output,"%s: Cannot open \"%s\" unit %ld (%ls).\n","TFTPClient",TIMERNAME,UNIT_VBLANK,error_text);

		D(("Cannot open '%s' unit %ld (%ls).",TIMERNAME,UNIT_VBLANK,error_text));

		goto out;
	}

	result = OK;

 out:

	return(result);
}

/****************************************************************************/

/* Clean up after the interval timer. */
void
timer_cleanup(void)
{
	ENTER();

	if(time_request != NULL)
	{
		stop_time();

		if(time_request->tr_node.io_Device != NULL)
		{
			CloseDevice((struct IORequest *)time_request);
			time_request->tr_node.io_Device = NULL;
		}

		DeleteIORequest((struct IORequest *)time_request);
		time_request = NULL;
	}

	if(time_port != NULL)
	{
		DeleteMsgPort(time_port);
		time_port = NULL;
	}

	LEAVE();
}
