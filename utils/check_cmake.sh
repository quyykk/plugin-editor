#!/bin/bash

# Script that checks if the CMake project is complete.
# TODO: If the project is incomplete generate a patch file to make it complete.

set -u pipefail -e
# Determine path of the current script, and go to the ES project root.
HERE=$(cd "$(dirname "$0")" && pwd)
cd "${HERE}"/.. || exit

ESTOP=$(pwd)
LIBLIST="${ESTOP}/source/CMakeLists.txt"
TESTLIST="${ESTOP}/tests/unit/CMakeLists.txt"

RESULT=0

for FILE in $(find source -type f -name "*.h" -o -name "*.cpp" -not -name main.cpp | sed s,^source/,, | sort)
do
  # Check if the file is present in the source list.
  if ! grep -Fq "${FILE}" "${LIBLIST}"; then
    if [ $RESULT -ne 1 ]; then
      echo -e "\033[1mMissing files in source/CMakeLists.txt:\033[0m"
    fi
    echo -e "${FILE}"
    RESULT=1
  fi
done

exit ${RESULT}
