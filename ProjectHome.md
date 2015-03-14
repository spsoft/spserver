SPServer is a server framework library written on C++ that implements the [Half-Sync/Half-Async](http://www.cs.wustl.edu/~schmidt/PDF/HS-HA.pdf) and [Leader/Follower](http://www.cs.wustl.edu/~schmidt/PDF/lf.pdf) patterns. It's based on [libevent](http://www.monkey.org/~provos/libevent/) in order to utilize the best I/O loop on any platform.

SPServer can simplify TCP server construction. It is a hybrid system between threaded and event-driven, and exploits the advantages of both programming models. It exposes a threaded programming style to programmers, while simultaneously using event-driven style to process network connection.

SPServer also include an embedded http server framework -- sphttp. sphttp can simplify to add web server functionality to any C++ app. It is useful for adding Web-based administration or statistics to any C++ program or becoming the server framework of XML-RPC or JSON-RPC.

SPServer is available through the [FreeBSD Ports Collection](http://www.freshports.org/net/spserver).

```
Changelog:

Changes in version 0.9.5 (12.13.2009)
-------------------------------------
* Fix memory leak in IOCP Server.
* Improve SP_DotTermMsgDecoder performance.

Changes in version 0.9.4 (03.15.2009)
-------------------------------------
* Added an embedded SMTP server based on spserver and some memory leak bugfixes.

Changes in version 0.9.3 (09.24.2008)
-------------------------------------
* Add judgement in SP_DefaultMsgDecoder to avoid dead loop
* Safe to close session with empty response
* Add toCloseList in SP_Response, using to terminate other sessions
* Set client ip in SP_Request when using SP_Dispatcher/SP_IocpDispatcher
* Porting to MacOS

Changes in version 0.9.2 (06.28.2008)
-------------------------------------
* Xyssl socket IO layer for spserver was added
* An abstract layer for socket IO was added for IOCP server framework
* Porting openssl plugin for IOCP server framework
* Porting sptunnel to win32

Changes in version 0.9.1 (05.24.2008)
-------------------------------------
* A win32 IOCP based server framework was added.
* A win32 IOCP based dispatcher was added.
* Improve demo programs' performance.

Changes in version 0.9.0 (04.19.2008)
-------------------------------------
* Porting to win32 with MSVC6
* Fix out of buffer write of SP_Buffer (From Jurgen Van Ham)
* Change default listen backlog to 1024
* Remove dependency of libevent/config.h

Changes in version 0.8.5 (10.27.2007)
-------------------------------------
* GNUTLS socket IO layer for spserver was added

Changes in version 0.8 (08.22.2007)
-------------------------------------
* Matrixssl socket IO layer for spserver was added
* Directory structure refactor

Changes in version 0.7.5 (08.12.2007)
-------------------------------------
* An ssl proxy, sptunnel, was added

Changes in version 0.7 (08.11.2007)
-------------------------------------
* A timer mechanism for SP_Dispatcher was added
* An abstract layer for socket IO was added
* An openssl socket IO layer for spserver was added

Changes in version 0.6 (07.01.2007)
-------------------------------------
* An Leader/Follower thread pool server was added

Changes in version 0.5 (06.23.2007)
-------------------------------------
* A socket dispatcher that applies a half-async/half-sync thread pool for server/client sockets was added.

Changes in version 0.4 (06.12.2007)
-------------------------------------
* A race condition in get/set of MsgDecoder was fixed.

Changes in version 0.3 (05.15.2007)
-------------------------------------
* A message block class was added to enable efficient manipulation of arbitrarily large messages without incurring much memory copying overhead.

Changes in version 0.2.1 (05.10.2007)
-------------------------------------
* Added an embedded HTTP server based on spserver and some minor bugfixes.

Changes in version 0.1 (03.14.2007)
-------------------------------------
* version 0.1 release
```