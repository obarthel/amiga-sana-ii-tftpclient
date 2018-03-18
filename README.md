# TFTP client program for the Amiga, using only the SANA-II network device driver API, and no TCP/IP stack

## 1. Introduction

This is a program which uses the "trivial file transfer protocol" (TFTP) for
either sending data from your Amiga to a remote TFTP server, or for requesting
data to be transmitted by a remote TFTP server to your Amiga.

The typical application for this program is for transmitting data between
your Amiga and "small computer" systems, for upgrading firmware or for
downloading log data. Because TFTP is such a simple protocol, it is often
easier to implement for small computer systems than the more complex
FTP, HTTP or SSH protocols which need a complete TCP/IP stack to work.


## 2. Preparations

This TFTP client program does not need a TCP/IP stack to work. More to the
point, **do not** use this program while a TCP/IP stack (*Roadshow*, *AmiTCP*,
*Miami*, etc.) is currently running! You should not be running both a TCP/IP
stack and this TFTP client in parallel, because both will end up stealing
packets which were really intended for the other guy.

You need Kickstart 2.04 or higher to use this program.

Before you can use this program you will need to know the following:

* Which IPv4 address is the remote TFTP server using?
* Which IPv4 address should the TFTP client use?
* Which network device driver and unit number should be used to access the network?

Both the IPv4 address of the TFTP server and the IPv4 address which the
Amiga will be using should reside within the same subnet.

Please note that because the TFTP client program does not use a TCP/IP stack,
there is no way for the program to find out which IPv4 address the Amiga
should be using, which is why you need to pick a suitable IPv4 address all
by yourself. This IPv4 address must not currently be used by a different
computer in your network.

Picking the right network device driver also requires knowledge of how
your Amiga is connected to the local network. The network device driver
might be `"ariadne.device"`, `"3c589.device"`, `"cnet.device"`, `"x-surf.device"`,
etc. Make sure that the device corresponds to Ethernet hardware which is
properly connected and operational.


## 3. How to use the program

This TFTP client program can only be used from the shell. It uses the following
command template:

```
DEVICE/K,UNIT/N,QUIET/S,VERBOSE/S,LOCALADDRESS/K,REMOTEPORT/N/K,
FILE=FROM/A,TO/A,OVERWRITE/S
```

The parameters `DEVICE/K` and `LOCALADDRESS/K` are mandatory. If your
Amiga would use the network device driver `"ariadne.device"`, unit 0 and
the IPv4 address 192.168.0.9 then you would start the TFTPClient command
with these parameters:

```
tftpclient device=ariadne.device localaddress=192.168.0.9
```

If the `UNIT/N` parameter is not provided, unit number 0 will be used
by default.


If you want to copy a file from a remote TFTP server to your Amiga, then
you would use the `FROM/A` and `TO/A` options like so (in one line):

```
tftpclient device=ariadne.device localaddress=192.168.0.9 from=192.168.0.15:/tftpboot/log.txt to=logfile
```

In this example the remote TFTP server is using the IPv4 address 192.168.0.15
and a file should be retrieved from it under the path name `"/tftpboot/log.txt"`.
That file should be stored on your Amiga as `"logfile"`.


To copy a file from your Amiga to a local TFTP server involves switching
the "from" and "to" options around (in one line):

```
tftpclient device=ariadne.device localaddress=192.168.0.9 from=firmware.zip to=192.168.0.15:/tftpboot/firmware.zip
```

You can omit the `DEVICE`, `UNIT` and `LOCALADDRESS` parameters altogether if you
set up the environment variables `TFTPDEVICE`, `TFTPUNIT` and `TFTPLOCALADDRESS`,
like so:

```
setenv TFTPDEVICE ariadne.device
setenv TFTPUNIT 0
setenv TFTPLOCALADDRESS 192.168.0.9
```

The tftpclient command examples above would then become much shorter:

```
tftpclient from=192.168.0.15:/tftpboot/log.txt to=logfile

tftpclient from=firmware.zip to=192.168.0.15:/tftpboot/firmware.zip
```

If you omit the `DEVICE` command line parameter, the program will check if
the `TFTPDEVICE` environment variable exists and use it instead. However,
if you provide a `DEVICE` parameter on the command line, then the program
will ignore the `TFTPDEVICE` environment variable altogether. It works the
same way with the `UNIT` parameter (and the `TFTPUNIT` environment variable)
and the `LOCALADDRESS` parameter (and the `TFTPLOCALADDRESS` environment
variable).


The remaining command line options work as follows:

`QUIET/S`

Use this to make the TFTPClient command to omit all progress and
error messages. This can be useful in connection with script
files which invoke the TFTPClient program.

`VERBOSE/S`

This is the counterpart to the `QUIET` option. It will enable
progress messages, reporting what exactly the TFTPClient is
currently doing. This also includes information about the data
being transmitted.

`REMOTEPORT=<Number>`

The TFTP server defaults to listen to port number 69. If necessary, you
can pick a different port number using the `REMOTEPORT` parameter. The valid
range of port numbers is 1..65535.

`OVERWRITE/S`

If the file which you are about to receive on your Amiga already exists
it will not be overwritten unless you use the `OVERWRITE` option.


The TFTPServer program will try as long as it takes to complete the transmission.
There is no time limit.

To stop the program before it has completed its task, press `Ctrl+C` in the shell
in which it is running, or use the `"Break"` shell command.


## 4. Limitations

It is not safe to use the TFTPClient command while a TCP/IP stack or *Envoy*
is currently running and using the same network device driver as TFTPClient.
If you still want to give it a try, be prepared for the whole transmission
to take much longer because TFTPClient and the TCP/IP stack will end up
stealing packets intended for the other guy. Who knows, there may be more
dire consequences for the TCP/IP stack.

Once started the TFTPClient command will try indefinitely to complete the
data transmission. There is no time limit (which is a limitation of sorts),
so you may need to watch out for the transmission to take too long. Press
`Ctrl+C` to abort it, or use the `"Break"` shell command.

The retransmission timeouts used by the TFTPClient command are hard-coded.
If the remote does not respond within about 1 second after a packet has
been sent to it, TFTPClient will resend that packet.

The TFTPClient command supports only the TFTP protocol (revision 2), as
described in RFC 1350. The TFTP option extensions described in, for example
RFC 2347, are not implemented.

Some network device drivers cannot be used safely with the TFTPClient command
because they do not handle opening and closing robustly. This may occur, for
example, for the `"a2065.device"` driver. Typically, network device drivers are
opened only once and then remain in use indefinitely by the TCP/IP stack or
by *Envoy*. The TFTPClient command, however, opens the respective network
device driver when needed and immediately closes it again as soon as it is
no longer required. This may cause the driver to hang, and it may even
crash the system.

All SANA-II compliant network drivers must support the buffer management
functions which are provided by the client at `OpenDevice()` time, namely the
functions associated with the `S2_CopyFromBuff` and `S2_CopyToBuff` tags.
Some drivers will, however, fail to work correctly unless the `S2_SANA2HOOK`
command is used and a hook function is installed. The TFTPClient will
provide such a hook function for the sake of convenience and stability.
Testing revealed at least one AmigaOS4 driver which will cause both the
TFTPClient command and the driver itself to crash unless the hook function is provided.

The TFTP protocol only supports sending and receiving files (see RFC 1350).
It is not a generalized file transfer protocol such as FTP or SFTP and
cannot be used interactively to, for example, list directory contents or
both receive and transmit file data in a single session. You are really
restricted to either sending or receiving file data. This is by design, not
by omission.

Many TFTP servers impose restrictions on how a TFTP client may access the
data stored on them. Some servers may require that you specify the complete
file system path under which to find and access a file, e.g. `"/tftpboot/file"`
will work, but `"file"` will not. Some servers require that for a file to be
uploaded by a TFTP client, a file by that name must already exist on the
server (i.e. you cannot upload arbitrary files). You may need to read up on
the restrictions imposed by the TFTP server which you want to use, because
the TFTPClient program has no way of knowing them in advance.

Because only 512 bytes of data may be transmitted to the remote server,
this restricts the length and the name of the path name, too.


## 5. Contacting the author

You can reach me as follows:

```
Olaf Barthel
Gneisenaustr. 43
D-31275 Lehrte
Federal Republic of Germany
```

My e-mail address is:

```
obarthel at gmx dot net
```

Bug fixes, bug reports and enhancements welcome!
