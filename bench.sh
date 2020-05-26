#!/usr/bin/env bash

set -x

IP=$1
CMD=$2
port=54322


function invoke {
	if [ $CMD == "garble" ]
	then
		sleep 15
	fi

	sudo free
	sudo sync
	echo 3 | sudo tee /proc/sys/vm/drop_caches
	sudo free
	echo $port
	$2 ./bin/mage $CMD $1/aspirin_${i} ${IP}:${port} > logs/aspirin_${3}_${i}_${CMD}.log
}

#for i in 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432 67108864
for i in 131072 33554432
do
	echo $i

	sudo swapoff -a
	sudo swapon /dev/nvme0n1p3
	port=$((port+1))
	invoke ~/work/files/unbounded "sudo cgexec -g memory:1gb" os_t1
	sudo swapoff -a
	port=$((port+1))
	invoke ~/work/files/unbounded "sudo" unbounded_t1
	port=$((port+1))
	invoke ~/work/files/1gb "sudo cgexec -g memory:1gb" mage_t1
done
