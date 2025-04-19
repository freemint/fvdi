#!/bin/sh -x

SERVER=web196@server43.webgo24.de
UPLOAD_DIR=$SERVER:/home/www/snapshots

if [ "${DEPLOY_ARCHIVE}" = "" ]
then
	# zip is default
	DEPLOY_ARCHIVE="zip"
fi

toolsuffix=${CROSS_TOOL##*-}

FT2=
if test "${FT2_VERSION}" != ""; then FT2="-ft${FT2_VERSION}"; fi

if [ "${CPU_TARGET}" != "" ]
then
	ARCHIVE_NAME="${PROJECT_NAME}-${PROJECT_VERSION}-${SHORT_ID}-${CPU_TARGET}.${DEPLOY_ARCHIVE}"
	LATEST_NAME="${PROJECT_NAME}-latest${FT2}-${CPU_TARGET}.${DEPLOY_ARCHIVE}"
else
	ARCHIVE_NAME="${PROJECT_NAME}-${PROJECT_VERSION}-${SHORT_ID}.${DEPLOY_ARCHIVE}"
	LATEST_NAME="${PROJECT_NAME}-latest.${DEPLOY_ARCHIVE}"
fi
ARCHIVE_PATH="${DEPLOY_DIR}/${ARCHIVE_NAME}"

mkdir -p "${DEPLOY_DIR}"

if [ "${DEPLOY_ARCHIVE}" = "tar.bz2" ]
then
	cd ${INSTALL_DIR} && tar cjf ${ARCHIVE_PATH} *
elif [ "${DEPLOY_ARCHIVE}" = "tar.xz" ]
then
	cd ${INSTALL_DIR} && tar cJf ${ARCHIVE_PATH} *
elif [ "${DEPLOY_ARCHIVE}" = "tar.gz" ]
then
	cd ${INSTALL_DIR} && tar czf ${ARCHIVE_PATH} *
else
	cd $(dirname ${INSTALL_DIR}) && zip -r -9 ${ARCHIVE_PATH} $(basename ${INSTALL_DIR})
fi

cd -


eval "$(ssh-agent -s)"

PROJECT_DIR="$PROJECT_NAME"
case $PROJECT_DIR in
	m68k-atari-mint-gcc) PROJECT_DIR=gcc ;;
	m68k-atari-mint-binutils-gdb) PROJECT_DIR=binutils ;;
esac

upload_file() {
	local from="$1"
	local to="$2"
	for i in 1 2 3
	do
		scp -o "StrictHostKeyChecking no" "$from" "$to"
		[ $? = 0 ] && return 0
		sleep 1
	done
	exit 1
}

link_file() {
	local from="$1"
	local to="$2"
	for i in 1 2 3
	do
		ssh -o "StrictHostKeyChecking no" $SERVER -- "cd www/snapshots/${PROJECT_DIR}; ln -sf $from $to"
		[ $? = 0 ] && return 0
		sleep 1
	done
	exit 1
}

upload_file "$ARCHIVE_PATH" "${UPLOAD_DIR}/${PROJECT_DIR}/${ARCHIVE_NAME}"
link_file "$ARCHIVE_NAME" "${LATEST_NAME}"

echo ${PROJECT_NAME}-${PROJECT_VERSION}-${SHORT_ID} > .latest_version
upload_file .latest_version "${UPLOAD_DIR}/${PROJECT_DIR}/.latest_version"
