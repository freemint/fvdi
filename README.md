[![Build Status](https://github.com/freemint/fvdi/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/freemint/fvdi/actions) 

Latest snapshot: [Download](https://tho-otto.de/snapshots/fvdi/)

# fvdi
fVDI fork with additional fixes and drivers.

## How to build
```
cd fvdi
make CPU=<CPU type> V=1
make CPU=<CPU type> DESTDIR=<install dir> install
```

Where `<CPU type>` is one of `v4e` (for ColdFire) or any other m68k-atari-mint
CPU target (`000` for 68000, `020` for 68020 etc).

To get truetype support: download a [freetype source archive](https://download.savannah.gnu.org/releases/freetype/), untar it somewhere, and create a symlink fvdi/modules/ft2/freetype to the top level directory.
