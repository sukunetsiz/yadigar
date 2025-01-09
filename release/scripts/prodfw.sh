#!/bin/bash

INDEX="BETA"
PRODDIRS="jade jade1.1 jade2.0"
BUILDDIRS="./build_*"
FW_PATTERN="*_fw.bin"
HASH_PATTERN="*_fw.bin.hash"

SIGNED_BINARY="./jade_signed.bin"

STAGING="staging"
UPLOAD="upload"

if [ -z "${1}" ]
then
  echo "Usage ${0} <version>"
  exit 1
fi
WORKING_DIR="${STAGING}/${1}"

# Relative paths from where it will be referenced in
# jade/release/staging/<working dir>/<hw flavour>/<build flavour>
FWPREP="../../../../../tools/fwprep.py"

pushd "${WORKING_DIR}"
for proddir in ${PRODDIRS}
do
  pushd "${proddir}"
  for dir in ${BUILDDIRS}
  do
    pushd "${dir}"
    "${FWPREP}" "${SIGNED_BINARY}" ..
    popd
  done

  ls ${FW_PATTERN} > "${INDEX}"
  cp ${FW_PATTERN} ${HASH_PATTERN} "${INDEX}" "../../${UPLOAD}/${proddir}"
  popd
done
popd
