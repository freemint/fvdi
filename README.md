[![Build Status](https://github.com/freemint/fvdi/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/freemint/fvdi/actions) 

Latest snapshot: [Download](https://tho-otto.de/snapshots/fvdi/)

# fvdi
fVDI fork with additional fixes and drivers.

- builds for ColdFire (using [MicroAPL's PortAsm](http://microapl.com/Porting/ColdFire/pacf_download.html)) depending on CPU setting
- CPU can be set with setting the CPU variable
- Makefiles are sensitive to "CPU=v4e" (ColdFire build) or any other m68k-atari-mint CPU target ("000", "020", etc.)
- to get truetype support: download a [freetype source archive](https://download.savannah.gnu.org/releases/freetype/), untar it somewhere, and create a symlink fvdi/modules/ft2/freetype to the top level directory
