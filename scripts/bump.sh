#!/bin/bash
#
# bump.sh
# Copyright (C) 2019 Morten Houmøller Nygaard <morten@mortz.dk>
#
# Distributed under terms of the MIT license.
#

# Function that checks whether command is available and installed
function is_installed()
{
    if ! [ -x "$(command -v $1)" ]; then
        echo "ERROR: '$1' is not installed." >&2
        exit 1
    fi
}

is_installed "git";
is_installed "sed";
is_installed "find";

# Accepts a version string and prints it incremented by one.
# Usage: increment_version <version> [<position>] [<leftmost>]
increment_version() {
   local usage=" USAGE: $FUNCNAME [-l] [-t] <version> [<position>] [<leftmost>]
           -l : remove leading zeros
           -t : drop trailing zeros
    <version> : The version string.
   <position> : Optional. The position (starting with one) of the number
                within <version> to increment.  If the position does not
                exist, it will be created.  Defaults to last position.
   <leftmost> : The leftmost position that can be incremented.  If does not
                exist, position will be created.  This right-padding will
                occur even to right of <position>, unless passed the -t flag."

   # Get flags.
   local flag_remove_leading_zeros=0
   local flag_drop_trailing_zeros=0
   while [ "${1:0:1}" == "-" ]; do
      if [ "$1" == "--" ]; then shift; break
      elif [ "$1" == "-l" ]; then flag_remove_leading_zeros=1
      elif [ "$1" == "-t" ]; then flag_drop_trailing_zeros=1
      else echo -e "Invalid flag: ${1}\n$usage"; return 1; fi
      shift; done

   # Get arguments.
   if [ ${#@} -lt 1 ]; then echo "$usage"; return 1; fi
   local v="${1}"             # version string
   local targetPos=${2-last}  # target position
   local minPos=${3-${2-0}}   # minimum position

   # Split version string into array using its periods.
   local IFSbak; IFSbak=IFS; IFS='.' # IFS restored at end of func to
   read -ra v <<< "$v"               #  avoid breaking other scripts.

   # Determine target position.
   if [ "${targetPos}" == "last" ]; then
      if [ "${minPos}" == "last" ]; then minPos=0; fi
      targetPos=$((${#v[@]}>${minPos}?${#v[@]}:$minPos)); fi
   if [[ ! ${targetPos} -gt 0 ]]; then
      echo -e "Invalid position: '$targetPos'\n$usage"; return 1; fi
   (( targetPos--  )) || true # offset to match array index

   # Make sure minPosition exists.
   while [ ${#v[@]} -lt ${minPos} ]; do v+=("0"); done;

   # Increment target position.
   v[$targetPos]=`printf %0${#v[$targetPos]}d $((10#${v[$targetPos]}+1))`;

   # Remove leading zeros, if -l flag passed.
   if [ $flag_remove_leading_zeros == 1 ]; then
      for (( pos=0; $pos<${#v[@]}; pos++ )); do
         v[$pos]=$((${v[$pos]}*1)); done; fi

   # If targetPosition was not at end of array, reset following positions to
   #   zero (or remove them if -t flag was passed).
   if [[ ${flag_drop_trailing_zeros} -eq "1" ]]; then
        for (( p=$((${#v[@]}-1)); $p>$targetPos; p-- )); do unset v[$p]; done
   else for (( p=$((${#v[@]}-1)); $p>$targetPos; p-- )); do v[$p]=0; done; fi

   echo "${v[*]}"
   IFS=IFSbak
   return 0
}

POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    --major)
    position=1
    shift # past argument
    shift # past value
    ;;
    --minor)
    position=2
    shift # past argument
    shift # past value
    ;;
    --patch)
    position=3
    shift # past argument
    shift # past value
    ;;
    --default)
    position=3
    shift # past argument
    ;;
    *)    # unknown option
    POSITIONAL+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

last_version=`git describe --abbrev=0 --tags`
new_version=`increment_version ${last_version#?} $position`
new_version="v${new_version}"

sed -i "/Current Version: /{s/\*\*$last_version\*\*/\*\*$new_version\*\*/g}" README.md
sed -i "/PROJECT_NUMBER\s*=/{s/$last_version/$new_version/g}" conf/doxyfile.conf

echo "$new_version"
