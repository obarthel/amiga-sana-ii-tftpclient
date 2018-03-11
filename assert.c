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

#include <dos/dos.h>

/****************************************************************************/

#define __NOLIBBASE__
#define __USE_INLINE__

#include <proto/exec.h>
#include <proto/dos.h>

/****************************************************************************/

#include <string.h>
#include <stdarg.h>

/****************************************************************************/

#if defined(__amigaos4__)

/****************************************************************************/

#define kprintf		DebugPrintF
#define kputc(c)	DebugPrintF("%lc",(LONG)(c))

/****************************************************************************/

#define IExec ((struct ExecIFace *)(*(struct ExecBase **)4)->MainInterface)

/****************************************************************************/

#else

/****************************************************************************/

extern struct Library * SysBase;

/****************************************************************************/

#define VARARGS68K
#define ASM __asm
#define REG(r,p) register __##r p

/****************************************************************************/

extern void __stdargs kprintf(const char *,...);
extern void __stdargs kputc(char c);

/****************************************************************************/

#endif /* __amigaos4__ */

/****************************************************************************/

#define DEBUGLEVEL_OnlyAsserts	0
#define DEBUGLEVEL_Reports	1
#define DEBUGLEVEL_CallTracing	2

/****************************************************************************/

int __debug_level__ = DEBUGLEVEL_CallTracing;

/****************************************************************************/

void _SETPROGRAMNAME(char *name);
int _SETDEBUGLEVEL(int level);
int _GETDEBUGLEVEL(void);
void _PUSHDEBUGLEVEL(int level);
void _POPDEBUGLEVEL(void);
void _INDENT(void);
void _SHOWVALUE(unsigned long value, int size, const char *name, const char *file, int line);
void _SHOWPOINTER(void *pointer, const char *name, const char *file, int line);
void _SHOWMACADDRESS(const void *addr,const char *name,const char *file,int line);
void _SHOWIPADDRESS(const void *a,const char *name,const char *file,int line);
void _SHOWSTRING(const char *string, const char *name, const char *file, int line);
void _SHOWMSG(const char *string, const char *file, int line);
void _DPRINTF_HEADER(const char *file, int line);
void VARARGS68K _DPRINTF(const char *fmt, ...);
void VARARGS68K _DLOG(const char *fmt, ...);
void _ENTER(const char *file, int line, const char *function);
void _LEAVE(const char *file, int line, const char *function);
void _RETURN(const char *file, int line, const char *function, unsigned long result);
void _ASSERT(int x, const char *xs, const char *file, int line, const char *function);

/****************************************************************************/

static int indent_level = 0;

static char program_name[40];
static int program_name_len = 0;

/****************************************************************************/

void
_SETPROGRAMNAME(char *name)
{
	if(name != NULL && name[0] != '\0')
	{
		program_name_len = strlen(name);
		if(program_name_len >= (int)sizeof(program_name))
			program_name_len = sizeof(program_name)-1;

		strncpy(program_name,name,program_name_len);
		program_name[program_name_len] = '\0';
	}
	else
	{
		program_name_len = 0;
	}
}

/****************************************************************************/

int
_SETDEBUGLEVEL(int level)
{
	int old_level = __debug_level__;

	__debug_level__ = level;

	return(old_level);
}

/****************************************************************************/

int
_GETDEBUGLEVEL(void)
{
	return(__debug_level__);
}

/****************************************************************************/

static int previous_debug_level = -1;

void
_PUSHDEBUGLEVEL(int level)
{
	previous_debug_level = _SETDEBUGLEVEL(level);
}

void
_POPDEBUGLEVEL(void)
{
	if(previous_debug_level != -1)
	{
		_SETDEBUGLEVEL(previous_debug_level);

		previous_debug_level = -1;
	}
}

/****************************************************************************/

void
_INDENT(void)
{
	if(program_name_len > 0)
		kprintf("(%s) ",program_name);

	if(__debug_level__ >= DEBUGLEVEL_CallTracing)
	{
		int i;

		for(i = 0 ; i < indent_level ; i++)
			kprintf("   ");
	}
}

/****************************************************************************/

void
_SHOWVALUE(
	unsigned long value,
	int size,
	const char *name,
	const char *file,
	int line)
{
	if(__debug_level__ >= DEBUGLEVEL_Reports)
	{
		char *fmt;

		switch(size)
		{
			case 1:

				fmt = "%s:%ld:%s = %ld, 0x%02lx";
				break;

			case 2:

				fmt = "%s:%ld:%s = %ld, 0x%04lx";
				break;

			default:

				fmt = "%s:%ld:%s = %ld, 0x%08lx";
				break;
		}

		_INDENT();

		kprintf(fmt,file,line,name,value,value);

		if(size == 1 && value < 256)
		{
			if(value < ' ' || (value >= 127 && value < 160))
				kprintf(", '\\x%02lx'",value);
			else
				kprintf(", '%lc'",value);
		}

		kprintf("\n");
	}
}

/****************************************************************************/

void
_SHOWPOINTER(
	void *pointer,
	const char *name,
	const char *file,
	int line)
{
	if(__debug_level__ >= DEBUGLEVEL_Reports)
	{
		char *fmt;

		_INDENT();

		if(pointer != NULL)
			fmt = "%s:%ld:%s = 0x%08lx\n";
		else
			fmt = "%s:%ld:%s = NULL\n";

		kprintf(fmt,file,line,name,pointer);
	}
}

/****************************************************************************/

void
_SHOWMACADDRESS(
	const void *a,
	const char *name,
	const char *file,
	int line)
{
	if(__debug_level__ >= DEBUGLEVEL_Reports)
	{
		unsigned char * address = (unsigned char *)a;
		char *fmt;

		_INDENT();

		if(a!= NULL)
			fmt = "%s:%ld:%s = %02lx:%02lx:%02lx:%02lx:%02lx:%02lx\n";
		else
			fmt = "%s:%ld:%s = NULL\n";

		kprintf(fmt,file,line,name,
			address[0],address[1],address[2],address[3],address[4],address[5]);
	}
}

/****************************************************************************/

void
_SHOWIPADDRESS(
	const void *a,
	const char *name,
	const char *file,
	int line)
{
	if(__debug_level__ >= DEBUGLEVEL_Reports)
	{
		unsigned char * address = (unsigned char *)a;
		char *fmt;

		_INDENT();

		if(a!= NULL)
			fmt = "%s:%ld:%s = %ld.%ld.%ld.%ld\n";
		else
			fmt = "%s:%ld:%s = NULL\n";

		kprintf(fmt,file,line,name,
			address[0],address[1],address[2],address[3]);
	}
}

/****************************************************************************/

void
_SHOWSTRING(
	const char *string,
	const char *name,
	const char *file,
	int line)
{
	if(__debug_level__ >= DEBUGLEVEL_Reports)
	{
		_INDENT();
		kprintf("%s:%ld:%s = 0x%08lx \"%s\"\n",file,line,name,string,string);
	}
}

/****************************************************************************/

void
_SHOWMSG(
	const char *string,
	const char *file,
	int line)
{
	if(__debug_level__ >= DEBUGLEVEL_Reports)
	{
		_INDENT();
		kprintf("%s:%ld:%s\n",file,line,string);
	}
}

/****************************************************************************/

void
_DPRINTF_HEADER(
	const char *file,
	int line)
{
	if(__debug_level__ >= DEBUGLEVEL_Reports)
	{
		_INDENT();
		kprintf("%s:%ld:",file,line);
	}
}

static void ASM
putch(REG(d0, UBYTE c))
{
	if(c != '\0')
		kputc(c);
}

void VARARGS68K
_DPRINTF(const char *fmt,...)
{
	if(__debug_level__ >= DEBUGLEVEL_Reports)
	{
		va_list args;

		#if defined(__amigaos4__)
		{
			va_startlinear(args,fmt);
			RawDoFmt((char *)fmt,va_getlinearva(args,void *),(VOID (*)())putch,NULL);
			va_end(args);
		}
		#else
		{
			va_start(args,fmt);
			RawDoFmt((char *)fmt,args,(VOID (*)())putch,NULL);
			va_end(args);
		}
		#endif /* __amigaos4__ */

		kprintf("\n");
	}
}

void VARARGS68K
_DLOG(const char *fmt,...)
{
	if(__debug_level__ >= DEBUGLEVEL_Reports)
	{
		va_list args;

		#if defined(__amigaos4__)
		{
			va_startlinear(args,fmt);
			RawDoFmt((char *)fmt,va_getlinearva(args,void *),(VOID (*)())putch,NULL);
			va_end(args);
		}
		#else
		{
			va_start(args,fmt);
			RawDoFmt((char *)fmt,args,(VOID (*)())putch,NULL);
			va_end(args);
		}
		#endif /* __amigaos4__ */
	}
}

/****************************************************************************/

void
_ENTER(
	const char *file,
	int line,
	const char *function)
{
	if(__debug_level__ >= DEBUGLEVEL_CallTracing)
	{
		_INDENT();
		kprintf("%s:%ld:Entering %s\n",file,line,function);
	}

	indent_level++;
}

void
_LEAVE(
	const char *file,
	int line,
	const char *function)
{
	indent_level--;

	if(__debug_level__ >= DEBUGLEVEL_CallTracing)
	{
		_INDENT();
		kprintf("%s:%ld: Leaving %s\n",file,line,function);
	}
}

void
_RETURN(
	const char *file,
	int line,
	const char *function,
	unsigned long result)
{
	indent_level--;

	if(__debug_level__ >= DEBUGLEVEL_CallTracing)
	{
		_INDENT();
		kprintf("%s:%ld: Leaving %s (result 0x%08lx, %ld)\n",file,line,function,result,result);
	}
}

/****************************************************************************/

void
_ASSERT(
	int x,
	const char *xs,
	const char *file,
	int line,
	const char *function)
{
	#ifdef CONFIRM
	{
		STATIC BOOL ScrollMode	= FALSE;
		STATIC BOOL BatchMode	= FALSE;

		if(BatchMode == FALSE)
		{
			if(x == 0)
			{
				kprintf("%s:%ld:Expression `%s' failed assertion in %s().\n",
				        file,
				        line,
				        xs,
				        function);

				if(ScrollMode == FALSE)
				{
					ULONG Signals;

					SetSignal(0,SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_E);

					kprintf(" ^C to continue, ^D to enter scroll mode, ^E to enter batch mode\r");

					Signals = Wait(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_E);

					if(Signals & SIGBREAKF_CTRL_D)
					{
						ScrollMode = TRUE;

						kprintf("Ok, entering scroll mode\033[K\n");
					}
					else if (Signals & SIGBREAKF_CTRL_E)
					{
						BatchMode = TRUE;

						kprintf("Ok, entering batch mode\033[K\n");
					}
					else
					{
						/* Continue */

						kprintf("\033[K\r");
					}
				}
			}
		}
	}
	#else
	{
		if(x == 0)
		{
			_INDENT();
			kprintf("%s:%ld:Expression `%s' failed assertion in %s().\n",
			        file,
			        line,
			        xs,
			        function);
		}
	}
	#endif	/* CONFIRM */
}
