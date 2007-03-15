
#--------------------------------------------------------------------

CC = gcc
AR = ar cru
CFLAGS = -Wall -D_REENTRANT -D_GNU_SOURCE -g
SOFLAGS = -shared -fPIC
LDFLAGS =

LINKER = $(CC)
LINT = lint -c
RM = /bin/rm -f

ifeq ($(origin version), undefined)
	version = 0.1
endif

LIBEVENT_INCL = -I../libevent/
LIBEVENT_LIB  = -L./ -levent

CFLAGS  += $(LIBEVENT_INCL)
LDFLAGS += $(LIBEVENT_LIB) -lpthread -lresolv

#--------------------------------------------------------------------

LIBOBJS = sputils.o spthreadpool.o event_msgqueue.o spbuffer.o sphandler.o \
	spmsgdecoder.o spresponse.o sprequest.o \
	spexecutor.o spsession.o speventcb.o spserver.o

TARGET =  libspserver.so \
		testecho testthreadpool testsmtp testchat teststress

#--------------------------------------------------------------------

all: $(TARGET)

libspserver.so: $(LIBOBJS)
	$(LINKER) $(SOFLAGS) $^ -o $@

testthreadpool: testthreadpool.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspserver -o $@

testsmtp: testsmtp.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspserver -o $@

testchat: testchat.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspserver -o $@

teststress: teststress.o
	$(LINKER) $(LDFLAGS) $^ -L. -levent -o $@

testecho: testecho.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspserver -o $@

dist: clean spserver-$(version).src.tar.gz

spserver-$(version).src.tar.gz:
	@ls | grep -v CVS | grep -v .so | sed 's:^:spserver-$(version)/:' > MANIFEST
	@(cd ..; ln -s spserver spserver-$(version))
	(cd ..; tar cvf - `cat spserver/MANIFEST` | gzip > spserver/spserver-$(version).src.tar.gz)
	@(cd ..; rm spserver-$(version))

clean:
	@( $(RM) *.o vgcore.* core core.* $(TARGET) )

#--------------------------------------------------------------------

# make rule
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@	

%.o : %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@	

