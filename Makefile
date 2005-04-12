ALL=ttdsrv ttdconsole

all: $(ALL)

DIET?=/opt/diet/bin/diet
CC?=gcc
CFLAGS=-Wall -W -pipe -fomit-frame-pointer -Os

CROSS=
#LDFLAGS=-s

ttdsrv: ttdsrv.o 
	$(DIET) $(CROSS)$(CC) $(LDFLAGS) -o $@ $^

ttdconsole: ttdconsole.o
	$(DIET) $(CROSS)$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(DIET) $(CROSS)$(CC) $(CFLAGS) -c $<

.PHONY: all clean install
clean:
	rm -f *.o $(ALL)

install:
	install -m 755 ttdsrv $(DESTDIR)/usr/bin
	install -m 755 ttdconsole $(DESTDIR)/usr/bin



