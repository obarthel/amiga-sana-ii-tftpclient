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

#ifndef DEVICES_SANA2_H
#include <devices/sana2.h>
#endif /* DEVICES_SANA2_H */

/****************************************************************************/

#include "macros.h"

#include "network-tftp.h"

/****************************************************************************/

#include "error-codes.h"

/****************************************************************************/

/* Looks up the descriptive text for a TFTP protocol error code.
 * Returns NULL if no such text is available.
 */
const char *
get_tftp_error_text(int error)
{
	static const struct { int error; const char * text; } tab[] =
	{
		{ TFTP_ERROR_UNDEF,		"Unspecified error type" },
		{ TFTP_ERROR_NOTFOUND,	"File not found" },
		{ TFTP_ERROR_ACCESS,	"Access violation" },
		{ TFTP_ERROR_NOSPACE,	"Disk full or allocation exceeded" },
		{ TFTP_ERROR_BADOP,		"Illegal TFTP operation" },
		{ TFTP_ERROR_BADID,		"Unknown transfer ID" },
		{ TFTP_ERROR_EXISTS,	"File already exists", },
		{ TFTP_ERROR_NOUSER,	"No such user" }
	};

	const char * result = NULL;
	int i;

	for(i = 0 ; i < (int)NUM_ENTRIES(tab) ; i++)
	{
		if(tab[i].error == error)
		{
			result = tab[i].text;
			break;
		}
	}

	return(result);
}

/****************************************************************************/

/* Looks up the descriptive text for an Amiga device error code.
 * Returns NULL if no such text is available.
 */
const char *
get_io_error_text(int error)
{
	static const struct { int error; const char * text; } tab[] =
	{
		{ IOERR_OPENFAIL,		"Device/unit failed to open" },
		{ IOERR_ABORTED,		"Request terminated early" },
		{ IOERR_NOCMD,			"Command not supported by device" },
		{ IOERR_BADLENGTH,		"Not a valid length" },
		{ IOERR_BADADDRESS,		"Invalid address" },
		{ IOERR_UNITBUSY,		"Device opens OK, but requested unit is busy"},
		{ IOERR_SELFTEST,		"Hardware failed self-test", },
		{ S2ERR_NO_RESOURCES,	"Resource allocation failure" },
		{ S2ERR_BAD_ARGUMENT,	"Bad argument" },
		{ S2ERR_BAD_STATE,		"Inappropriate state" },
		{ S2ERR_BAD_ADDRESS,	"Bad address" },
		{ S2ERR_MTU_EXCEEDED,	"Maximum transmission unit exceeded" },
		{ S2ERR_NOT_SUPPORTED,	"Command not supported by hardware" },
		{ S2ERR_SOFTWARE,		"Software error detected" },
		{ S2ERR_OUTOFSERVICE,	"Driver is offline" },
		{ S2ERR_TX_FAILURE,		"Transmission attempt failed" }
	};

	const char * result = NULL;
	int i;

	for(i = 0 ; i < (int)NUM_ENTRIES(tab) ; i++)
	{
		if(tab[i].error == error)
		{
			result = tab[i].text;
			break;
		}
	}

	return(result);
}

/****************************************************************************/

/* Looks up the descriptive text for an Amiga SANA-II wire error code.
 * Returns NULL if no such text is available.
 */
const char *
get_wire_error_text(int error)
{
	static const struct { int error; const char * text; } tab[] =
	{
		{ S2WERR_GENERIC_ERROR,			"No specific information available" },
		{ S2WERR_NOT_CONFIGURED,		"Unit not configured" },
		{ S2WERR_UNIT_ONLINE,			"Unit is currently online" },
		{ S2WERR_UNIT_OFFLINE,			"Unit is currently offline" },
		{ S2WERR_ALREADY_TRACKED,		"Protocol already tracked" },
		{ S2WERR_NOT_TRACKED,			"Protocol not tracked" },
		{ S2WERR_BUFF_ERROR,			"Buffer management function returned error" },
		{ S2WERR_SRC_ADDRESS,			"Source address problem" },
		{ S2WERR_DST_ADDRESS,			"Destination address problem" },
		{ S2WERR_BAD_BROADCAST,			"Broadcast address problem" },
		{ S2WERR_BAD_MULTICAST,			"Multicast address problem" },
		{ S2WERR_MULTICAST_FULL,		"Multicast address list is full" },
		{ S2WERR_BAD_EVENT,				"Unsupported event class" },
		{ S2WERR_BAD_STATDATA,			"StatData failed sanity check" },
		{ S2WERR_IS_CONFIGURED,			"Attempt to configure twice" },
		{ S2WERR_NULL_POINTER,			"NULL pointer detected" },
		{ S2WERR_TOO_MANY_RETIRES,		"Transmission failed - too many retries" },
		{ S2WERR_RCVREL_HDW_ERR,		"Driver fixable hardware error" },
		{ S2WERR_UNIT_DISCONNECTED,		"Unit currently not connected" },
		{ S2WERR_UNIT_CONNECTED,		"Unit currently connected" },
		{ S2WERR_INVALID_OPTION,		"Invalid option rejected" },
		{ S2WERR_MISSING_OPTION,		"Mandatory option missing" },
		{ S2WERR_AUTHENTICATION_FAILED,	"Could not log in" }
	};

	const char * result = NULL;
	int i;

	for(i = 0 ; i < (int)NUM_ENTRIES(tab) ; i++)
	{
		if(tab[i].error == error)
		{
			result = tab[i].text;
			break;
		}
	}

	return(result);
}
