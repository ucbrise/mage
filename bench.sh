#!/usr/bin/env bash

set -x

SWAP_DEVICE=$1
ROLE=$2

function invoke {
	if [ $ROLE == "garbler" ]
	then
		sleep 160
	else
		sleep 125
	fi

	sudo free
	sudo sync
	echo 3 | sudo tee /proc/sys/vm/drop_caches
	sudo free
	$1 ./mage $2 $3 $4 0 $5 > ~/work/logs/${5}_${6}_${7}.log
	if [ $ROLE == "garbler" ]
	then
		diff ${5}_0.output ${5}_0.expected > ~/work/logs/${5}_${6}_${7}.result
	fi
}

function run_benchmark {
	# $1 is protocol (e.g., halfgates)
	# $2 is the problem name
	# $3 is the problem size
	# $4 is the number of trials
	for trial in $(seq $4)
	do
		sudo swapoff -a
		./example_input $2 $3 1
		./planner $2 ../config_unbounded.yaml $ROLE 0 $3
		sudo swapon ${SWAP_DEVICE}
		invoke "sudo cgexec -g memory:memprog" $1 ../config_unbounded.yaml $ROLE ${2}_${3} os t$trial
		sudo swapoff -a
		invoke "sudo" $1 ../config_unbounded.yaml $ROLE ${2}_${3} unbounded t$trial
		./planner $2 ../config.yaml $ROLE 0 $3
		invoke "sudo cgexec -g memory:memprog" $1 ../config.yaml $ROLE ${2}_${3} mage t$trial
	done
}

mkdir -p ~/work/logs

run_benchmark halfgates merge_sorted 1048576 10
run_benchmark halfgates full_sort 1048576 10
run_benchmark halfgates loop_join 2048 10
run_benchmark halfgates matrix_vector_mult 8192 10
run_benchmark halfgates binary_fc_layer 16384 10
# run_benchmark halfgates merge_sorted 1024 3
# run_benchmark halfgates full_sort 1024 3
# run_benchmark halfgates loop_join 256 3
# run_benchmark halfgates matrix_vector_mult 256 3
# run_benchmark halfgates binary_fc_layer 512 3
