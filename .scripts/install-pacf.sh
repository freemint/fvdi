#!/bin/bash -eux
# -e: Exit immediately if a command exits with a non-zero status.
# -u: Treat unset variables as an error when substituting.
# -x: Display expanded script commands

if test "${FT2_VERSION}" != ""
then
	cd /tmp
	subdir=""
	if test ${FT2_VERSION} = "2.2.1"; then subdir="freetype-old/"; fi
	if test ${FT2_VERSION} = "2.5.5"; then subdir="freetype-old/"; fi
	wget -q -O - "https://download.savannah.gnu.org/releases/freetype/${subdir}freetype-${FT2_VERSION}.tar.gz" | tar xzf -
	mv "freetype-${FT2_VERSION}" freetype
	cd -

	cd fvdi/modules/ft2 && ln -s "/tmp/freetype" && cd -
fi

case ${CPU_TARGET} in
*v4e*)
	# needed for pacf, which is a 32bit executable
	sudo dpkg --add-architecture i386
	sudo apt-get update
	# libc6:amd64 seems to be outdated
	sudo apt-get install --only-upgrade libc6
	# install 32-bit libc
	sudo apt-get install libc6:i386

	PACF="/tmp/pacf/bin/pacf"
	echo "PACF=$PACF" >> $GITHUB_ENV
	mkdir -p "/tmp/pacf/bin"
	cd "/tmp/pacf/bin"
	wget -q -O - http://microapl.com/Porting/ColdFire/Download/pacflin.gz | gzip -dc > pacf
	chmod 755 pacf
	cd -
	;;
esac
