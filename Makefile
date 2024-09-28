# Makefile (this file is inspired by "http://www.jwz.org/dadadodo/" source)

SHELL           = /bin/sh
CC              = gcc -Wall -Wstrict-prototypes -Wnested-externs -Wno-format
CFLAGS          = -g -std=c99
LDFLAGS         =
DEFS            = -DGETTIMEOFDAY_TWO_ARGS -DHAVE_UNISTD_H
LIBS            = -lm

DEPEND          = makedepend
DEPEND_FLAGS    =
DEPEND_DEFINES  =

srcdir          = .
INCLUDES        = -I$(srcdir)

SRCS            = main.c ppm.c pgm.c
OBJS            = main.o ppm.o pgm.o
EXE             = ppmtools

HDRS            = ppm.h pgm.h version.h
MEN             =
EXTRAS          = makefile README

TARFILES        = $(EXTRAS) $(SRCS) $(HDRS) $(MEN)
TAR             = tar
COMPRESS        = gzip --verbose --best
COMPRESS_EXT    = gz

all: $(EXE)

clean:
	-rm -f *.o *.out $(EXE)

distclean: clean
	-rm -f *~ "#"*

depend:
	$(DEPEND) -s '# DO NOT DELETE: updated by make depend'		   \
	$(DEPEND_FLAGS) -- $(INCLUDES) $(DEFS) $(DEPEND_DEFINES) $(CFLAGS) \
	-- $(SRCS)

TAGS: tags
tags:
	find $(srcdir) -name '*.[chly]' -print | xargs etags -a

.c.o:
	$(CC) -c $(INCLUDES) $(DEFS) $(CFLAGS) $<

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

main.o: ppm.h pgm.h version.h
ppm.o: ppm.h
pgm.o: pgm.h


tar:
	@NAME=`sed -n							    \
  's/.* \([0-9]\.[0-9][0-9]*\).*/ppmtools-\1/p' version.h` ;		    \
  rm -f $$NAME ; ln -s . $$NAME ;					    \
  echo creating tar file $${NAME}.tar.$(COMPRESS_EXT)... ;		    \
   $(TAR) -vchf - `echo $(TARFILES)				    	    \
   | sed "s|^|$$NAME/|g; s| | $$NAME/|g" `				    \
   | $(COMPRESS) > $${NAME}.tar.$(COMPRESS_EXT) ;			    \
  rm $$NAME ;								    \
  echo "" ;								    \
  ls -lgF $${NAME}.tar.$(COMPRESS_EXT) ;				    \
  echo "" ;

