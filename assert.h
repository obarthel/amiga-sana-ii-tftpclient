/*
 * :ts=4
 *
 * TFTP client program for the Amiga, using only the SANA-II network
 * device driver API, and no TCP/IP stack
 *
 * The "trivial file transfer protocol" is anything but trivial
 * to implement...
 *
 * Copyright © 2016-2018 by Olaf Barthel <obarthel at gmx dot net>
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

/* IMPORTANT: If DEBUG is redefined, it must happen only here. This
 *            will cause all modules to depend upon it to be rebuilt
 *            by the smakefile (that is, provided the smakefile has
 *            all the necessary dependency lines in place).
 */

#ifdef DEBUG
#undef DEBUG
#endif /* DEBUG */

/*#define DEBUG*/

/****************************************************************************/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif /* EXEC_TYPES_H */

/****************************************************************************/

#ifdef ASSERT
#undef ASSERT
#endif	/* ASSERT */

/****************************************************************************/

#define DEBUGLEVEL_OnlyAsserts	0
#define DEBUGLEVEL_Reports	1
#define DEBUGLEVEL_CallTracing	2

/****************************************************************************/

#define PUSH_ASSERTS()	PUSHDEBUGLEVEL(DEBUGLEVEL_OnlyAsserts)
#define PUSH_REPORTS()	PUSHDEBUGLEVEL(DEBUGLEVEL_Reports)
#define PUSH_CALLS()	PUSHDEBUGLEVEL(DEBUGLEVEL_CallTracing)
#define PUSH_ALL()	PUSH_CALLS()
#define POP()		POPDEBUGLEVEL()

/****************************************************************************/

#if defined(DEBUG)
extern int __debug_level__;
#endif /* DEBUG */

/****************************************************************************/

#if defined(DEBUG) && defined(__SASC)
 void _ASSERT(int x,const char *xs,const char *file,int line,const char *function);
 void _SHOWVALUE(unsigned long value,int size,const char *name,const char *file,int line);
 void _SHOWPOINTER(void *p,const char *name,const char *file,int line);
 void _SHOWSTRING(const char *string,const char *name,const char *file,int line);
 void _SHOWMSG(const char *msg,const char *file,int line);
 void _SHOWMACADDRESS(const void *a,const char *name,const char *file,int line);
 void _SHOWIPADDRESS(const void *a,const char *name,const char *file,int line);
 void _ENTER(const char *file,int line,const char *function);
 void _LEAVE(const char *file,int line,const char *function);
 void _RETURN(const char *file,int line,const char *function,unsigned long result);
 void _DPRINTF_HEADER(const char *file,int line);
 void _DPRINTF(const char *format,...);
 void _DLOG(const char *format,...);
 int  _SETDEBUGLEVEL(int level);
 void _PUSHDEBUGLEVEL(int level);
 void _POPDEBUGLEVEL(void);
 int  _GETDEBUGLEVEL(void);
 void _SETPROGRAMNAME(char *name);

 #define ASSERT(x)		_ASSERT((int)(x),#x,__FILE__,__LINE__,__FUNC__)
 #define ENTER()		do { if(__debug_level__ >= DEBUGLEVEL_CallTracing) _ENTER(__FILE__,__LINE__,__FUNC__); } while(0)
 #define LEAVE()		do { if(__debug_level__ >= DEBUGLEVEL_CallTracing) _LEAVE(__FILE__,__LINE__,__FUNC__); } while(0)
 #define RETURN(r)		do { if(__debug_level__ >= DEBUGLEVEL_CallTracing) _RETURN(__FILE__,__LINE__,__FUNC__,(unsigned long)r); } while(0)
 #define SHOWVALUE(v)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWVALUE(v,sizeof(v),#v,__FILE__,__LINE__); } while(0)
 #define SHOWPOINTER(p)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWPOINTER(p,#p,__FILE__,__LINE__); } while(0)
 #define SHOWMACADDRESS(a)	do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWMACADDRESS(a,#a,__FILE__,__LINE__); } while(0)
 #define SHOWIPADDRESS(a)	do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWIPADDRESS(a,#a,__FILE__,__LINE__); } while(0)
 #define SHOWSTRING(s)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWSTRING(s,#s,__FILE__,__LINE__); } while(0)
 #define SHOWMSG(s)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWMSG(s,__FILE__,__LINE__); } while(0)
 #define D(s)			do { if(__debug_level__ >= DEBUGLEVEL_Reports) { _DPRINTF_HEADER(__FILE__,__LINE__); _DPRINTF s; } } while(0)
 #define PRINTHEADER()		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _DPRINTF_HEADER(__FILE__,__LINE__); } while(0)
 #define PRINTF(s)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _DLOG s; } while(0)
 #define LOG(s)			do { if(__debug_level__ >= DEBUGLEVEL_Reports) { _DPRINTF_HEADER(__FILE__,__LINE__); _DLOG("<%s()>:",__FUNC__); _DLOG s; } } while(0)
 #define SETDEBUGLEVEL(l)	_SETDEBUGLEVEL(l)
 #define PUSHDEBUGLEVEL(l)	_PUSHDEBUGLEVEL(l)
 #define POPDEBUGLEVEL()	_POPDEBUGLEVEL()
 #define SETPROGRAMNAME(n)	_SETPROGRAMNAME(n)
 #define GETDEBUGLEVEL()	_GETDEBUGLEVEL()

 #undef DEBUG
 #define DEBUG 1
#elif (defined(DEBUG) && defined(__GNUC__))
 void _ASSERT(int x,const char *xs,const char *file,int line,const char *function);
 void _SHOWVALUE(unsigned long value,int size,const char *name,const char *file,int line);
 void _SHOWPOINTER(void *p,const char *name,const char *file,int line);
 void _SHOWMACADDRESS(const void *addr,const char *name,const char *file,int line);
 void _SHOWIPADDRESS(const void *a,const char *name,const char *file,int line);
 void _SHOWSTRING(const char *string,const char *name,const char *file,int line);
 void _SHOWMSG(const char *msg,const char *file,int line);
 void _ENTER(const char *file,int line,const char *function);
 void _LEAVE(const char *file,int line,const char *function);
 void _RETURN(const char *file,int line,const char *function,unsigned long result);
 void _DPRINTF_HEADER(const char *file,int line);
#if defined(__amigaos4__)
 void VARARGS68K _DPRINTF(const char *format,...);
 void VARARGS68K _DLOG(const char *format,...);
#else
 void _DPRINTF(const char *format,...);
 void _DLOG(const char *format,...);
#endif /* __amigaos4__ */
 int  _SETDEBUGLEVEL(int level);
 void _PUSHDEBUGLEVEL(int level);
 void _POPDEBUGLEVEL(void);
 int  _GETDEBUGLEVEL(void);
 void _SETPROGRAMNAME(char *name);

 #define ASSERT(x)		_ASSERT((int)(x),#x,__FILE__,__LINE__,__PRETTY_FUNCTION__)
 #define ENTER()		do { if(__debug_level__ >= DEBUGLEVEL_CallTracing) _ENTER(__FILE__,__LINE__,__PRETTY_FUNCTION__); } while(0)
 #define LEAVE()		do { if(__debug_level__ >= DEBUGLEVEL_CallTracing) _LEAVE(__FILE__,__LINE__,__PRETTY_FUNCTION__); } while(0)
 #define RETURN(r)		do { if(__debug_level__ >= DEBUGLEVEL_CallTracing) _RETURN(__FILE__,__LINE__,__PRETTY_FUNCTION__,(unsigned long)r); } while(0)
 #define SHOWVALUE(v)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWVALUE(v,sizeof(v),#v,__FILE__,__LINE__); } while(0)
 #define SHOWPOINTER(p)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWPOINTER(p,#p,__FILE__,__LINE__); } while(0)
 #define SHOWMACADDRESS(a)	do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWMACADDRESS(a,#a,__FILE__,__LINE__); } while(0)
 #define SHOWIPADDRESS(a)	do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWIPADDRESS(a,#a,__FILE__,__LINE__); } while(0)
 #define SHOWSTRING(s)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWSTRING(s,#s,__FILE__,__LINE__); } while(0)
 #define SHOWMSG(s)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _SHOWMSG(s,__FILE__,__LINE__); } while(0)
 #define D(s)			do { if(__debug_level__ >= DEBUGLEVEL_Reports) { _DPRINTF_HEADER(__FILE__,__LINE__); _DPRINTF s;} } while(0)
 #define PRINTHEADER()		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _DPRINTF_HEADER(__FILE__,__LINE__); } while(0)
 #define PRINTF(s)		do { if(__debug_level__ >= DEBUGLEVEL_Reports) _DLOG s; } while(0)
 #define LOG(s)			do { if(__debug_level__ >= DEBUGLEVEL_Reports) { _DPRINTF_HEADER(__FILE__,__LINE__); _DLOG("<%s()>:",__PRETTY_FUNCTION__); _DLOG s; } } while(0)
 #define SETDEBUGLEVEL(l)	_SETDEBUGLEVEL(l)
 #define PUSHDEBUGLEVEL(l)	_PUSHDEBUGLEVEL(l)
 #define POPDEBUGLEVEL()	_POPDEBUGLEVEL()
 #define SETPROGRAMNAME(n)	_SETPROGRAMNAME(n)
 #define GETDEBUGLEVEL()	_GETDEBUGLEVEL()

 #undef DEBUG
 #define DEBUG 1
#else
 #define ASSERT(x)		((void)0)
 #define ENTER()		((void)0)
 #define LEAVE()		((void)0)
 #define RETURN(r)		((void)0)
 #define SHOWVALUE(v)		((void)0)
 #define SHOWPOINTER(p)		((void)0)
 #define SHOWMACADDRESS(a)	((void)0)
 #define SHOWIPADDRESS(a)	((void)0)
 #define SHOWSTRING(s)		((void)0)
 #define SHOWMSG(s)		((void)0)
 #define D(s)			((void)0)
 #define PRINTHEADER()		((void)0)
 #define PRINTF(s)		((void)0)
 #define LOG(s)			((void)0)
 #define SETDEBUGLEVEL(l)	((void)0)
 #define PUSHDEBUGLEVEL(l)	((void)0)
 #define POPDEBUGLEVEL()	((void)0)
 #define SETPROGRAMNAME(n)	((void)0)
 #define GETDEBUGLEVEL()	(0)

 #ifdef DEBUG
 #undef DEBUG
 #endif /* DEBUG */

 #define DEBUG 0
#endif /* DEBUG */

/****************************************************************************/
