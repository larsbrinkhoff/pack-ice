CC = gcc
CFLAGS = -Wall -g

PROGS = icecat freeze thaw
ICELIB = libice.a
ICELIBOBJS = ice_info.o ice_crunch.o ice_decrunch.o

all: $(PROGS)

icecat: icecat.o $(ICELIB)
	gcc -o icecat icecat.o $(ICELIB)

freeze: freeze.o $(ICELIB)
	gcc -o freeze freeze.o $(ICELIB)

thaw: thaw.o $(ICELIB)
	gcc -o thaw thaw.o $(ICELIB)

$(ICELIB): $(ICELIBOBJS)
	rm -f $(ICELIB)
	ar cr $(ICELIB) $(ICELIBOBJS)
	ranlib $(ICELIB)

clean:
	rm -f $(PROGS) $(ICELIB) $(ICELIBOBJS) icecat.o freeze.o thaw.o
