#!/bin/bash
set -e -u
REPO="https://github.com/A4091/mounter"
TMPDIR=./tmp
CURRENT=$(cat upstream-commit)

rm -rf "${TMPDIR}"
mkdir -p "${TMPDIR}"

git clone "${REPO}" "${TMPDIR}"
NEW=$(git -C "${TMPDIR}" rev-parse HEAD)

if [[ "${NEW}" = "${CURRENT}" ]]; then
    echo "Already up to date."
    rm -rf "${TMPDIR}"
    exit 0
fi

git -C "${TMPDIR}" format-patch "${CURRENT}" --stdout > "${TMPDIR}/mounter.patch"
git am --directory "3rdparty/mounter" "${TMPDIR}/mounter.patch"
echo "${NEW}" > upstream-commit
rm -rf "${TMPDIR}"