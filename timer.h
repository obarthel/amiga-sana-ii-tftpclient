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

#ifndef _TIMER_H
#define _TIMER_H

/****************************************************************************/

#ifndef DEVICES_TIMER_H
#include <devices/timer.h>
#endif /* DEVICES_TIMER_H */

#ifndef DOS_DOS_H
#include <dos/dos.h>
#endif /* DOS_DOS_H */

/****************************************************************************/

#ifndef _ARGS_H
#include "args.h"
#endif /* _ARGS_H */

/****************************************************************************/

extern struct MsgPort *		time_port;
extern struct timerequest *	time_request;
extern BOOL					time_in_use;

/****************************************************************************/

extern void stop_time(void);
extern void start_time(ULONG seconds);
extern int timer_setup(BPTR error_output, const struct cmd_args * args);
extern void timer_cleanup(void);

/****************************************************************************/

#endif /* _TIMER_H */
