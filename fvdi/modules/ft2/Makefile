#
# to build from the builds/freemint directory call
#
#  make
#
# Your programs source code should start with this
# (uncomment the parts you do not need to keep the program small):
# ---8<---
#define FT_USE_AUTOHINT // autohinter
#define FT_USE_RASTER   // monochrome rasterizer
#define FT_USE_SMOOTH   // anti-aliasing rasterizer
#define FT_USE_TT       // truetype font driver
#define FT_USE_T1       // type1 font driver
#define FT_USE_T42      // type42 font driver
#define FT_USE_T1CID    // cid-keyed type1 font driver
#define FT_USE_CFF      // opentype font driver
#define FT_USE_BDF      // bdf bitmap font driver
#define FT_USE_PCF      // pcf bitmap font driver
#define FT_USE_PFR      // pfr font driver
#define FT_USE_WINFNT   // windows .fnt|.fon bitmap font driver
#include "FT:src/base/ftinit.c"
# ---8<---
#
# link your programs with libft2.a and either ftsystem.o or ftsystempure.o
# (and either ftdebug.o or ftdebugpure.o if you enabled FT_DEBUG_LEVEL_ERROR or
# FT_DEBUG_LEVEL_TRACE in include/freetype/config/ftoption.h).

TARGET = libft2.a

all:	$(TARGET)

top_srcdir  = ../..
srcdir      = .
subdir      = ft2

include $(top_srcdir)/CONFIGVARS

ifndef FREETYPE_VERSION
ifneq (,$(wildcard $(ft2_srcdir)/include/freetype/freetype.h))
	FREETYPE_MAJOR := $(shell sed -n -e '/define FREETYPE_MAJOR/s/[^0-9]*//p' $(ft2_srcdir)/include/freetype/freetype.h)
	FREETYPE_MINOR := $(shell sed -n -e '/define FREETYPE_MINOR/s/[^0-9]*//p' $(ft2_srcdir)/include/freetype/freetype.h)
	FREETYPE_PATCH := $(shell sed -n -e '/define FREETYPE_PATCH/s/[^0-9]*//p' $(ft2_srcdir)/include/freetype/freetype.h)
else ifneq (,$(wildcard $(ft2_srcdir)/include/freetype.h))
	FREETYPE_MAJOR := $(shell sed -n -e '/define FREETYPE_MAJOR/s/[^0-9]*//p' $(ft2_srcdir)/include/freetype.h)
	FREETYPE_MINOR := $(shell sed -n -e '/define FREETYPE_MINOR/s/[^0-9]*//p' $(ft2_srcdir)/include/freetype.h)
	FREETYPE_PATCH := $(shell sed -n -e '/define FREETYPE_PATCH/s/[^0-9]*//p' $(ft2_srcdir)/include/freetype.h)
endif
FREETYPE_VERSION := $(shell printf "%d%03d%03dL" $(FREETYPE_MAJOR) $(FREETYPE_MINOR) $(FREETYPE_PATCH))
endif

CFLAGS = $(CPUOPTS) $(OPTS) $(FT2_WARNINGS) $(FT2_CFLAGS) -DFREETYPE_VERSION=$(FREETYPE_VERSION) -I$(top_srcdir)/include -I$(top_srcdir)/modules/include -I$(ft2_srcdir)/src

#
# FreeType2 library base
#
ftbase.o: $(ft2_srcdir)/src/base/ftbase.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftinit.o: $(ft2_srcdir)/src/base/ftinit.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftsystem.o: $(ft2_srcdir)/src/base/ftsystem.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftdebug.o: $(ft2_srcdir)/src/base/ftdebug.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library base extensions
#
ftglyph.o: $(ft2_srcdir)/src/base/ftglyph.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftbbox.o: $(ft2_srcdir)/src/base/ftbbox.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftmm.o: $(ft2_srcdir)/src/base/ftmm.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftsynth.o: $(ft2_srcdir)/src/base/ftsynth.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftbitmap.o: $(ft2_srcdir)/src/base/ftbitmap.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftstroke.o: $(ft2_srcdir)/src/base/ftstroke.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

# Since FreeType 2.6.x
ftfntfmt.o: $(ft2_srcdir)/src/base/ftfntfmt.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library autohinting module
#
autohint.o: $(ft2_srcdir)/src/autohint/autohint.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library autohinting module extensions
#
autofit.o: $(ft2_srcdir)/src/autofit/autofit.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library postscript hinting module
#
pshinter.o: $(ft2_srcdir)/src/pshinter/pshinter.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library PS support module
#
psaux.o: $(ft2_srcdir)/src/psaux/psaux.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library PS glyph names module
#
psnames.o: $(ft2_srcdir)/src/psnames/psnames.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library monochrome raster module
#
raster.o: $(ft2_srcdir)/src/raster/raster.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library anti-aliasing raster module
#
smooth.o: $(ft2_srcdir)/src/smooth/smooth.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library 'sfnt' module
#
sfnt.o: $(ft2_srcdir)/src/sfnt/sfnt.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library glyph and image caching system (still experimental)
#
ftcache.o: $(ft2_srcdir)/src/cache/ftcache.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library OpenType font driver
#
cff.o: $(ft2_srcdir)/src/cff/cff.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

# opentype validator
otvalid.o: $(ft2_srcdir)/src/otvalid/otvalid.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library TrueType font driver
#
truetype.o: $(ft2_srcdir)/src/truetype/truetype.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

# TrueType GX validator
gxvalid.o: $(ft2_srcdir)/src/gxvalid/gxvalid.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<


#
# FreeType2 library Type1 font driver
#
type1.o: $(ft2_srcdir)/src/type1/type1.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library Type42 font driver
#
type42.o: $(ft2_srcdir)/src/type42/type42.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library CID-keyed Type1 font driver
#
type1cid.o: $(ft2_srcdir)/src/cid/type1cid.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library BDF bitmap font driver
#
bdf.o: $(ft2_srcdir)/src/bdf/bdf.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library PCF bitmap font driver
#
pcf.o: $(ft2_srcdir)/src/pcf/pcf.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftbzip2.o: $(ft2_srcdir)/src/bzip2/ftbzip2.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftgzip.o: $(ft2_srcdir)/src/gzip/ftgzip.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

ftlzw.o: $(ft2_srcdir)/src/lzw/ftlzw.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library PFR font driver
#
pfr.o: $(ft2_srcdir)/src/pfr/pfr.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

#
# FreeType2 library Windows FNT/FON bitmap font driver
#
winfnt.o: $(ft2_srcdir)/src/winfonts/winfnt.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

gemfnt.o: $(ft2_srcdir)/src/gemfonts/gemfnt.c
	$(AM_V_CC)$(CC) -c $(CFLAGS) -o $@ $<

BASEOBJ = ftinit.o ftbase.o ftglyph.o ftbbox.o ftmm.o ftsynth.o ftbitmap.o ftstroke.o
ifneq (,$(wildcard $(ft2_srcdir)/src/base/ftfntfmt.c))
BASEOBJ += ftfntfmt.o
endif

#AHINTOBJ = autohint.o autofit.o
AHINTOBJ = autofit.o

PSOBJ = psaux.o psnames.o pshinter.o

RASTEROBJ = raster.o smooth.o

GZOBJ = ftgzip.o ftlzw.o

# pcf.o  - needs GZLib and ZLib
PCFOBJ = pcf.o
FONTDOBJ = cff.o type1.o type42.o type1cid.o truetype.o bdf.o pfr.o winfnt.o $(PCFOBJ)
ifneq (,$(wildcard $(ft2_srcdir)/src/gemfonts/gemfnt.c))
FONTDOBJ += gemfnt.o
endif

OBJS = $(BASEOBJ) $(AHINTOBJ) $(PSOBJ) $(RASTEROBJ) sfnt.o ftcache.o $(FONTDOBJ) $(GZOBJ)

CSRC = ft2.c atari2u.c ft2_ftsystem.c ft2_ftdebug.c
OBJS += $(CSRC:.c=.o)

include $(top_srcdir)/DEPENDENCIES

ifneq ($(ft2_srcdir),)
$(TARGET): $(OBJS)
	$(AM_V_AR)$(AR) crs $@ $(OBJS)
else
$(TARGET):
	@:
endif

clean::
	$(RM) *.o *.a

install::
	@:
