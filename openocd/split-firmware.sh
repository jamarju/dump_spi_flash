#!/bin/bash
#
# Split a 8 MB NOR dump into the default Unifi AP partitions: 
#
# -rw-r--r-- 1 root root  262144 Jun  5 16:04 mtdblock0
# -rw-r--r-- 1 root root   65536 Jun  5 16:04 mtdblock1
# -rw-r--r-- 1 root root 1048576 Jun  5 16:04 mtdblock2
# -rw-r--r-- 1 root root 6684672 Jun  5 16:04 mtdblock3
# -rw-r--r-- 1 root root  262144 Jun  5 16:05 mtdblock4
# -rw-r--r-- 1 root root   65536 Jun  5 16:05 mtdblock5
#

if [ $# -ne 1 ]
then
  echo "ERROR: split.sh dumpfile.bin"
  exit 1
fi

dd bs=64k if=$1 of=mtdblock0 count=4
dd bs=64k if=$1 of=mtdblock1 count=1 skip=4
dd bs=64k if=$1 of=mtdblock2 count=16 skip=5
dd bs=64k if=$1 of=mtdblock3 count=102 skip=21
dd bs=64k if=$1 of=mtdblock4 count=4 skip=123
dd bs=64k if=$1 of=mtdblock5 count=1 skip=127
