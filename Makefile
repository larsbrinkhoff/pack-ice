CC = gcc
CFLAGS = -Wall -g

PROGS = icecat freeze melt
ICELIB = libice.a
ICELIB_OBJS = ice_info.o ice_crunch.o ice_decrunch.o

all: $(PROGS)

icecat: icecat.o $(ICELIB)
	gcc -o icecat icecat.o $(ICELIB)

freeze: freeze.o $(ICELIB)
	gcc -o freeze freeze.o $(ICELIB)

melt: melt.o $(ICELIB)
	gcc -o melt melt.o $(ICELIB)

$(ICELIB): $(ICELIB_OBJS)
	rm -f $(ICELIB)
	ar cr $(ICELIB) $(ICELIB_OBJS)
	ranlib $(ICELIB)

clean:
	rm -f $(PROGS) $(ICELIB) $(ICELIBOBJS) icecat.o freeze.o melt.o
