
#--------------------------------------------------------------------

CC = gcc
AR = ar cru
CFLAGS = -Wall -D_REENTRANT -D_GNU_SOURCE -g -fPIC
SOFLAGS = -shared
LDFLAGS = -lstdc++

LINKER = $(CC)
LINT = lint -c
RM = /bin/rm -f

OPENSSL_INCL = -I/usr/include/openssl/
OPENSSL_LIB  = -L/usr/lib -lssl -lcrypto

LIBEVENT_LIB  = -L../../libevent -levent

CFLAGS  += $(OPENSSL_INCL) -I../
LDFLAGS += $(LIBEVENT_LIB) $(OPENSSL_LIB) -lpthread -lresolv

#--------------------------------------------------------------------

LIBOBJS = spopenssl.o

TARGET =  libspopenssl.so sptunnel \
	testechos testhttps

#--------------------------------------------------------------------

all: $(TARGET)

libspopenssl.so: $(LIBOBJS)
	$(LINKER) $(SOFLAGS) $^ -o $@

testechos: testechos.o
	$(LINKER) $(LDFLAGS) $^ -L../ -lspserver -L. -lspopenssl -o $@

testhttps: testhttps.o
	$(LINKER) $(LDFLAGS) $^ -L../ -lspserver -L. -lspopenssl -o $@

sptunnel: sptunnelimpl.o sptunnel.o
	$(LINKER) $(LDFLAGS) $^ -L../ -lspserver -L. -lspopenssl -o $@

clean:
	@( $(RM) *.o vgcore.* core core.* $(TARGET) )

#--------------------------------------------------------------------

# make rule
%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@	

%.o : %.cpp
	$(CC) $(CFLAGS) -c $^ -o $@	

