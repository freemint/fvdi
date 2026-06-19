[![Build Status](https://github.com/freemint/fvdi/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/freemint/fvdi/actions) 

* Latest 000: [no FreeType](https://atari.joska.no/snapshots/fvdi/fvdi-latest-000.zip), [FreeType 2.2.1](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.2.1-000.zip), [2.5.5](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.5.5-000.zip), [2.8.1](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.8.1-000.zip), [2.10.4](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.10.4-000.zip)
* Latest 020: [no FreeType](https://atari.joska.no/snapshots/fvdi/fvdi-latest-020.zip), [FreeType 2.2.1](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.2.1-020.zip), [2.5.5](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.5.5-020.zip), [2.8.1](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.8.1-020.zip), [2.10.4](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.10.4-020.zip)
* Latest v4e: [no FreeType](https://atari.joska.no/snapshots/fvdi/fvdi-latest-v4e.zip), [FreeType 2.2.1](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.2.1-v4e.zip), [2.5.5](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.5.5-v4e.zip), [2.8.1](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.8.1-v4e.zip), [2.10.4](https://atari.joska.no/snapshots/fvdi/fvdi-latest-ft2.10.4-v4e.zip)
* [Archive](https://atari.joska.no/snapshots/fvdi/)

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
