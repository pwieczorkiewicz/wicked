
CFLAGS	= -Wall -Werror -g -O2 -D_GNU_SOURCE -I. -Iinclude

APPS	= wicked wickedd testing/xml-test testing/xpath-test

TGTLIBS	= libnetinfo.a \
	  libnetcf.a
	  # libnetinfo.so
# Public header files
LIBHDRS	= logging.h \
	  netinfo.h \
	  types.h \
	  util.h \
	  wicked.h \
	  xml.h \
	  xpath.h \
	  netcf.h
__LIBSRCS= \
	  config.c \
	  rest-api.c \
	  extension.c \
	  policy.c \
	  netinfo.c \
	  netconfig.c \
	  vlan.c \
	  bonding.c \
	  bridge.c \
	  state.c \
	  iflist.c \
	  ifconfig.c \
	  ifevent.c \
	  kernel.c \
	  address.c \
	  sysconfig.c \
	  sysfs.c \
	  libnetlink.c \
	  syntax.c \
	  backend-suse.c \
	  backend-redhat.c \
	  backend-netcf.c \
	  xml.c \
	  xml-reader.c \
	  xml-writer.c \
	  xpath.c \
	  xpath-fmt.c \
	  util.c \
	  socket.c \
	  logging.c
__NCFSRCS= \
	  netcf.c

OBJ	= obj
LIBSRCS	= $(addprefix src/,$(__LIBSRCS))
LIBOBJS	= $(addprefix $(OBJ)/lib/,$(__LIBSRCS:.c=.o))
SHLIBOBJS= $(addprefix $(OBJ)/shlib/,$(__LIBSRCS:.c=.o))
NCFSRCS	= $(addprefix src/,$(__NCFSRCS))
NCFOBJS	= $(addprefix $(OBJ)/netcf/,$(__NCFSRCS:.c=.o))
APPSRCS	= $(addsuffix .c,$(APPS))

all: $(TGTLIBS) $(APPS)

distclean clean::
	rm -f *.o *.a *.so $(APPS) core tags
	rm -rf $(OBJ)
	rm -f testing/*.o

distclean::
	rm -f .depend

install: install-files
	install -d -m 755 $(DESTDIR)/sbin
	install -s -m 555 wickedd wicked $(DESTDIR)/sbin
	install -d -m 755 $(DESTDIR)/usr/share/man/man{7,8}
	install -c -m 444 man/*.7 $(DESTDIR)/usr/share/man/man7
	install -c -m 444 man/*.8 $(DESTDIR)/usr/share/man/man8

install-files:
	install -d -m 755 $(DESTDIR)/etc/wicked
	install -m 644 etc/wicked/*.xml $(DESTDIR)/etc/wicked
	install -m 555 etc/wicked/wicked-dhcp4 $(DESTDIR)/etc/wicked

wicked: $(OBJ)/wicked.o $(TGTLIBS)
	$(CC) -o $@ $(CFLAGS) $(OBJ)/wicked.o -L. -lnetinfo

wickedd: $(OBJ)/wickedd.o $(TGTLIBS)
	$(CC) -o $@ $(CFLAGS) $(OBJ)/wickedd.o -L. -lnetinfo

test: $(OBJ)/test.o $(TGTLIBS)
	$(CC) -o $@ $(CFLAGS) $(OBJ)/test.o -L. -lnetinfo

testing/xml-test: testing/xml-test.o $(TGTLIBS)
	$(CC) -o $@ $(CFLAGS) testing/xml-test.o -L. -lnetinfo

testing/xpath-test: testing/xpath-test.o $(TGTLIBS)
	$(CC) -o $@ $(CFLAGS) testing/xpath-test.o -L. -lnetinfo

libnetinfo.a: $(LIBOBJS)
	ar cr $@ $(LIBOBJS)

libnetcf.a: $(NCFOBJS)
	ar cr $@ $(NCFOBJS)

libnetinfo.so: $(SHLIBOBJS)
	$(CC) $(CFLAGS) -shared -o $@ $(SHLIBOBJS)

depend:
	gcc $(CFLAGS) -M $(LIBSRCS) | sed 's:^[a-z]:$(OBJ)/lib/&:' > .depend
	gcc $(CFLAGS) -M $(NCFSRCS) | sed 's:^[a-z]:$(OBJ)/netcf/&:' >> .depend
	gcc $(CFLAGS) -M $(APPSRCS) | sed 's:^[a-z]:$(OBJ)/&:' >> .depend

$(OBJ)/%.o: %.c
	@test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/lib/%.o: src/%.c
	@test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ)/shlib/%.o: src/%.c
	@test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

$(OBJ)/netcf/%.o: src/%.c
	@test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

-include .depend

