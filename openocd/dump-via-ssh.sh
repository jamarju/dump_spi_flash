#!/bin/bash
#
# Dumps /dev/mtdblock* from the given UAP's IP. Uses the default username 'ubnt', change it
# below otherwise.

user=ubnt

if [ $# -ne 1 ]
then
  echo "ERROR: dump-via-ssh.sh ip_address"
  exit 1
fi

ipaddr=$1

# Dump each mtdblock separately:
#for i in 0 1 2 3 4 5; do ssh "$user@$ipaddr" cat /dev/mtdblock$i >mtdblock$i; done

ssh "$user@$ipaddr" cat /dev/mtdblock[012345] >"dump_$ipaddr.bin"
