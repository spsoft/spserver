
#--------------------------------------------------------------------

CC = gcc
AR = ar cru
CFLAGS = -Wall -D_REENTRANT -D_GNU_SOURCE -g -fPIC
SOFLAGS = -shared
LDFLAGS = -lstdc++ -lpthread -lresolv

LINKER = $(CC)
LINT = lint -c
RM = /bin/rm -f

LIBEVENT_INCL = -I$(HOME)/libevent/
LIBEVENT_LIB  = -L$(HOME)/libevent -levent

CFLAGS  += $(LIBEVENT_INCL)
LDFLAGS += $(LIBEVENT_LIB)

#--------------------------------------------------------------------

LIBOBJS = sputils.o spioutils.o spiochannel.o \
	spthreadpool.o event_msgqueue.o spbuffer.o sphandler.o \
	spmsgblock.o spmsgdecoder.o spresponse.o sprequest.o \
	spexecutor.o spsession.o speventcb.o spserver.o \
	spdispatcher.o splfserver.o \
	sphttpmsg.o sphttp.o

TARGET =  libspserver.so \
		testecho testthreadpool testsmtp testchat teststress testhttp \
		testhttpmsg testdispatcher testchat_d testunp

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

testhttp: testhttp.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspserver -o $@

testhttpmsg: sputils.o sphttpmsg.o testhttpmsg.o
	$(LINKER) $(LDFLAGS) $^ -o $@

testdispatcher: testdispatcher.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspserver -o $@

testchat_d: testchat_d.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspserver -o $@

testunp: testunp.o
	$(LINKER) $(LDFLAGS) $^ -L. -lspserver -o $@

clean:
	@( $(RM) *.o vgcore.* core core.* $(TARGET) )

#--------------------------------------------------------------------

# make rule
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@	

%.o : %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@	

