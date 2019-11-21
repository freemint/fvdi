#!make -f
#
# This software is licensed under the GNU General Public License.
# Please, see LICENSE.TXT for further information.
#

#
# gcc >= 2.95.3 (sparemint) version
#

TARGET = firebee.sys

all: $(TARGET)

top_srcdir = ../..

include $(top_srcdir)/CONFIGVARS

CPU     = v4e

ifeq ($(CPU),v4e)
	CPUFLAGS = -mcpu=547x
else
	CPUFLAGS = -m68$(CPU)
endif

M68K_ATARI_MINT_LDFLAGS += -Map mapfile

CFLAGS  = $(CPUFLAGS) -mshort -O2 -fomit-frame-pointer -I$(top_srcdir)/include -I$(top_srcdir)/drivers/include $(M68K_ATARI_MINT_CFLAGS) -Wall
ASFLAGS = $(CPUFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/drivers/include

SINCSRC = \
	$(top_srcdir)/include/compiler.inc \
	$(top_srcdir)/include/macros.inc \
	$(top_srcdir)/include/types.inc \
	$(top_srcdir)/include/vdi.inc \
	$(top_srcdir)/drivers/include/linea.inc \
	$(top_srcdir)/drivers/include/pixelmac.inc \
	$(top_srcdir)/drivers/1_plane/1_expand.inc

SSRC = \
	../common/c_common.s \
	../common/common.s \
	../common/clip.s \
	../common/lineachk.s

CSRC = \
	../common/init.c \
	../common/colours.c \
	fb_spec.c \
	firebee.c \
	modeline_vesa.c \
	firebee_pll.c \
	firebee_mouse.c \
	../16_bit/16b_exp.c \
	../16_bit/16b_blit.c \
	../16_bit/16b_line.c \
	../16_bit/16b_fill.c \
	../16_bit/16b_scr.c \
	../16_bit/16b_pal.c 

# handle DevPac -> gas conversions
# defines SSRC_GNU and SINCSRC_GNU variables
include $(top_srcdir)/utility/conv2gas/RULES

# need to convert the .inc file as well
$(SSRC_GNU): $(SINCSRC_GNU)

# engine binary objects
OBJECTS = $(SSRC_GNU:.gnu=.gnu.o) $(CSRC:.c=.o)

$(TARGET): depend $(OBJECTS) $(LIBFILES)
	$(LD) $(M68K_ATARI_MINT_LDFLAGS) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

depend: $(CSRC)
#	"$(CC) -MM" does not seem able to keep track of the directory of the
#	object file, so dependencies of files outside this directory are
#	never taken into account later. makedepend does this correctly.
#	Unfortunately, makedepend includes system headers in its
#	dependency lists, but you can't get everything...
#	$(CC) -MM $(CFLAGS) $(CSRC) > depend
	makedepend -f- -- $(CFLAGS) $(CSRC) > depend 2> /dev/null
	sed "s#\/usr[^ ]*\.h##g" < depend  > depend2
	sed "s#^.*: *\$$##"      < depend2 > depend
	rm depend2

strip:
	$(STRIP) --strip-all $(TARGET)

clean: clean_gnu
	rm -rf $(OBJECTS)
	rm depend

include depend