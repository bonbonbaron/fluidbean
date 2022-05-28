##############################
# Variables
##############################
CC             = cc
SRCDIR         = ./src
BUILDDIR       = ./build
LIBDIR         = ./lib
INCLUDEDIR     = ./src/include
INSTALLDIR     = /usr/local/lib
LIBBOTOX       = ./lib/libbotox.a
BOTOXSRC      = $(shell find ./src -name "*.c") 
#$(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.$(OBJEXT)))
BOTOX_OBJS     = $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(BOTOXSRC:.c=.o))
BOTOX_INCLUDES = $(shell find -name "*.h")
TGTINCLUDDIR = /usr/local/include/botox/

CFLAGS_COMMON  = -g -Wall \
							   -I$(INCLUDEDIR) -ffunction-sections \
							 	 -fdata-sections -s -fno-ident -fmerge-all-constants \
							 	 -fomit-frame-pointer -fno-stack-protector 
CFLAGS_TINY    = $(CFLAGS_COMMON) #-Os    #TODO: uncomment when ready for relase
CFLAGS_FAST    = $(CFLAGS_COMMON) #-Ofast    #TODO: uncomment when ready for relase
SDL_LFLAGS     = $(shell sdl2-config --libs)
LFLAGS         = -Wl,--gc-sections -Wl,-z,norelro \

##############################
# Archiver
##############################
$(LIBBOTOX): $(BUILDDIR) $(LIBDIR) $(BOTOX_OBJS) $(BOTOX_INCLUDES)
	ar rcs $(LIBBOTOX) $(BOTOX_OBJS)
	touch $@

##############################
# Compiler
##############################
$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(BOTOX_INCLUDES)
	$(CC) $(CFLAGS_FAST) -c $< -o $@
	touch $@
	
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(LIBDIR):
	mkdir -p $(LIBDIR)
##############################
.PHONY: install
install:
	mkdir -p $(TGTINCLUDDIR)
	cp $(BOTOX_INCLUDES) /usr/local/include/botox/
	cp $(LIBBOTOX) $(INSTALLDIR)

.PHONY: clean
clean:
	rm -f $(BOTOX_OBJS)
	rm -f $(LIBBOTOX)
