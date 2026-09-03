#!/bin/bash -eu
VERSION=$(git describe --tags --dirty | sed -r 's/^Release-//')
DEVICE_VERSION=$(echo ${VERSION} | awk -F'-' '{split($1,a,".");print a[1]}')
DEVICE_REVISION=$(echo ${VERSION} | awk -F'-' '{split($1,a,".");print a[2]}')
GIT_REF=$(git branch --show-current)-$(git rev-parse --short HEAD)
BUILD_DATE=$(date  +"%d.%m.%Y")

echo -ne "VERSION = ${DEVICE_VERSION}
_idstring: dc.b 'lide.device ${DEVICE_VERSION}.${DEVICE_REVISION} (${BUILD_DATE}) ${GIT_REF}', 0
" > tmp/version.i