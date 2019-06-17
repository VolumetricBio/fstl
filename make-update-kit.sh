#!/bin/bash

VERSION=1.0.1
PACKAGE_BUILD_ROOT=/home/lumen/Volumetric/LightField/packaging

#########################################################
##                                                     ##
##     No user-serviceable parts below this point.     ##
##                                                     ##
#########################################################

function clear () {
    echo -ne "\x1B[0m\x1B[H\x1B[J\x1B[3J"
}

function blue-bar () {
    echo -e "\r\x1B[1;37;44m$*\x1B[K\x1B[0m"
}

function blue-bar () {
    echo -e "\r\x1B[1;37;44m$*\x1B[K\x1B[0m" 1>&2
}

function red-bar () {
    echo -e "\r\x1B[1;33;41m$*\x1B[K\x1B[0m" 1>&2
}

function error-trap () {
    red-bar Failed\!
    exit 1
}

function usage () {
    cat <<EOF
Usage: $(basename $0) [-q] BUILDTYPE
Where: -q         build quietly
and:   BUILDTYPE  is one of
                  release  create a release-build kit
                  debug    create a debug-build kit
                  both     create both kits

If the build is successful, the requested upgrade kit(s) will be found in
  ${KIT_DIR}/lightfield-BUILDTYPE_${VERSION}_amd64.kit
EOF
}

trap error-trap ERR
set -e

LIGHTFIELD_SRC="/home/lumen/Volumetric/LightField"
PACKAGE_BUILD_DIR="${PACKAGE_BUILD_ROOT}/${VERSION}"
DEB_BUILD_DIR="${PACKAGE_BUILD_DIR}/deb"
LIGHTFIELD_PACKAGE="${DEB_BUILD_DIR}/lightfield-${VERSION}"
LIGHTFIELD_FILES="${LIGHTFIELD_PACKAGE}/files"

KIT_DIR="${PACKAGE_BUILD_DIR}/kit"

VERBOSE=-v
BUILDTYPE=

while [ -n "$1" ]
do
    case "$1" in
	"-q")
	    VERBOSE=
	;;

	"release" | "debug" | "both")
	    BUILDTYPE=$1
	;;

	*)
	    usage
	    exit 1
	;;
    esac
    shift
done

if [ -z "${BUILDTYPE}" ]
then
    usage
    exit 1
fi

if [ "${BUILDTYPE}" = "both" ]
then
    ARG=
    if [ -z "${VERBOSE}" ]
    then
        ARG=-q
    fi
    $0 ${ARG} release || exit $?
    $0 ${ARG} debug   || exit $?
    exit 0
fi

APT_CACHE_DIR="${PACKAGE_BUILD_DIR}/apt-cache"
REPO_DIR="${PACKAGE_BUILD_DIR}/repo"
DISTRIBUTION=cosmic

RELEASEDATE=$(date "+%Y-%m-%d")

clear

cd "${PACKAGE_BUILD_DIR}"

blue-bar • Creating LightField "${VERSION}" "${BUILDTYPE}"-build update kit

[ -d "${APT_CACHE_DIR}" ] && rm ${VERBOSE} -rf "${APT_CACHE_DIR}"
[ -d "${REPO_DIR}"      ] && rm ${VERBOSE} -rf "${REPO_DIR}"

mkdir ${VERBOSE} -p "${APT_CACHE_DIR}"
mkdir ${VERBOSE} -p "${REPO_DIR}/dists/${DISTRIBUTION}/main/binary-amd64"
mkdir ${VERBOSE} -p "${REPO_DIR}/dists/${DISTRIBUTION}/main/binary-all"
mkdir ${VERBOSE} -p "${KIT_DIR}"

install ${VERBOSE} -Dt "${REPO_DIR}/pool/main/f/" -m 644 "${LIGHTFIELD_SRC}/fonts-montserrat_7.200_all.deb"
install ${VERBOSE} -Dt "${REPO_DIR}/pool/main/l/" -m 644 "${DEB_BUILD_DIR}/lightfield-common_${VERSION}_all.deb"

if [ "${BUILDTYPE}" = "release" ]
then
    install ${VERBOSE} -Dt "${REPO_DIR}/pool/main/l/" -m 644 "${DEB_BUILD_DIR}/lightfield-release_${VERSION}_amd64.deb"
elif [ "${BUILDTYPE}" = "debug" ]
then
    install ${VERBOSE} -Dt "${REPO_DIR}/pool/main/l/" -m 644 "${DEB_BUILD_DIR}/lightfield-debug_${VERSION}_amd64.deb"
    install ${VERBOSE} -Dt "${REPO_DIR}/pool/main/l/" -m 644 "${DEB_BUILD_DIR}/lightfield-debug-dbgsym_${VERSION}_amd64.deb"
fi

apt-ftparchive                                                             \
    --config-file "${LIGHTFIELD_SRC}/apt-files/release.conf"               \
    --arch amd64                                                           \
    packages "${REPO_DIR}/pool"                                            \
    | xz -eT0                                                              \
    > "${REPO_DIR}/dists/${DISTRIBUTION}/main/binary-amd64/Packages.xz"

apt-ftparchive                                                             \
    --config-file "${LIGHTFIELD_SRC}/apt-files/release.conf"               \
    --arch all                                                             \
    packages "${REPO_DIR}/pool"                                            \
    | xz -eT0                                                              \
    > "${REPO_DIR}/dists/${DISTRIBUTION}/main/binary-all/Packages.xz"

apt-ftparchive                                                             \
    --config-file "${LIGHTFIELD_SRC}/apt-files/release.conf"               \
    --arch amd64                                                           \
    contents "${REPO_DIR}/pool"                                            \
    | xz -eT0                                                              \
    > "${REPO_DIR}/dists/${DISTRIBUTION}/Contents-amd64.xz"

apt-ftparchive                                                             \
    --config-file "${LIGHTFIELD_SRC}/apt-files/release.conf"               \
    --arch all                                                             \
    contents "${REPO_DIR}/pool"                                            \
    | xz -eT0                                                              \
    > "${REPO_DIR}/dists/${DISTRIBUTION}/Contents-all.xz"

apt-ftparchive                                                             \
    --config-file "${LIGHTFIELD_SRC}/apt-files/release.conf"               \
    --arch amd64                                                           \
    release "${REPO_DIR}/dists/${DISTRIBUTION}/main/binary-amd64"          \
    > "${REPO_DIR}/dists/${DISTRIBUTION}/main/binary-amd64/Release"

apt-ftparchive                                                             \
    --config-file "${LIGHTFIELD_SRC}/apt-files/release.conf"               \
    --arch all                                                             \
    release "${REPO_DIR}/dists/${DISTRIBUTION}/main/binary-all"            \
    > "${REPO_DIR}/dists/${DISTRIBUTION}/main/binary-all/Release"

apt-ftparchive                                                             \
    --config-file "${LIGHTFIELD_SRC}/apt-files/release.conf"               \
    release "${REPO_DIR}/dists/${DISTRIBUTION}"                            \
    > "${REPO_DIR}/dists/${DISTRIBUTION}/Release"

#apt-ftparchive                                                             \
#    --config-file "${LIGHTFIELD_SRC}/apt-files/release.conf"               \
#    generate <(                                                            \
#        sed                                                                \
#            -e "s:@@APT_CACHE_DIR@@:${APT_CACHE_DIR}:g"                    \
#            -e "s:@@DISTRIBUTION@@:${DISTRIBUTION}:g"                      \
#            -e "s:@@REPO_DIR@@:${REPO_DIR}:g"                              \
#            "${LIGHTFIELD_SRC}/apt-files/apt-ftparchive.conf.in"           \
#    )

gpg                                                                        \
    ${VERBOSE}                                                             \
    --batch                                                                \
    --detach-sign                                                          \
    --armor                                                                \
    --local-user "lightfield-repo-maint@volumetricbio.com"                 \
    --output "${REPO_DIR}/dists/${DISTRIBUTION}/Release.gpg"               \
    "${REPO_DIR}/dists/${DISTRIBUTION}/Release"

cd "${REPO_DIR}"

rm ${VERBOSE} -rf                                                          \
    "${APT_CACHE_DIR}"                                                     \
    "${REPO_DIR}/version.inf"                                              \
    "${REPO_DIR}/version.inf.sig"

sed                                                                        \
    -e "s/@@VERSION@@/${VERSION}/g"                                        \
    -e "s/@@BUILDTYPE@@/${BUILDTYPE}/g"                                    \
    -e "s/@@RELEASEDATE@@/${RELEASEDATE}/g"                                \
    "${LIGHTFIELD_SRC}/apt-files/version.inf.in"                           \
    > "${REPO_DIR}/version.inf"

gpg                                                                        \
    ${VERBOSE}                                                             \
    --batch                                                                \
    --detach-sign                                                          \
    --armor                                                                \
    --local-user "lightfield-packager@volumetricbio.com"                   \
    --output "${REPO_DIR}/version.inf.sig"                                 \
    "${REPO_DIR}/version.inf"

rm ${VERBOSE} -f                                                           \
    "${KIT_DIR}/lightfield-${BUILDTYPE}_${VERSION}_amd64.kit"              \
    "${KIT_DIR}/lightfield-${BUILDTYPE}_${VERSION}_amd64.kit.sig"

tar                                                                        \
    ${VERBOSE} ${VERBOSE}                                                  \
    -c                                                                     \
    -f "${KIT_DIR}/lightfield-${BUILDTYPE}_${VERSION}_amd64.kit"           \
    --owner=root                                                           \
    --group=root                                                           \
    *

cd ${KIT_DIR}

gpg                                                                        \
    ${VERBOSE}                                                             \
    --batch                                                                \
    --detach-sign                                                          \
    --armor                                                                \
    --local-user "lightfield-packager@volumetricbio.com"                   \
    --output "${KIT_DIR}/lightfield-${BUILDTYPE}_${VERSION}_amd64.kit.sig" \
    "${KIT_DIR}/lightfield-${BUILDTYPE}_${VERSION}_amd64.kit"

blue-bar ""
blue-bar "• Done!"
blue-bar ""
