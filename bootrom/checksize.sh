#!/bin/bash
RED='\033[1;31m'
RESET='\033[0m'

SIZE=$(stat -c %s $1)

if [[ ${SIZE} -gt 28668 ]]
then
    echo -e "${RED}$(basename $1) too large for ROM! ${SIZE} > 28668"
    exit 1
fi