#!/usr/bin/env bash

set -x

SWAP_DEVICE=$1
ROLE=$2
WORKER_ID=$3
NUM_WORKERS=$4

if [[ -z $NUM_WORKERS ]]
then
	echo "Usage:" $0 "os_swap_device garbler/evaluator worker_id num_workers"
	exit
fi

function invoke {
	if [[ $8 = true ]]
	then
		if [ $ROLE == "garbler" ]
		then
			sleep $(expr 160 + $(expr 20 "*" $WORKER_ID))
		else
			sleep $(expr 125 + $(expr 20 "*" $WORKER_ID))
		fi
	fi

	sudo free
	sudo sync
	echo 3 | sudo tee /proc/sys/vm/drop_caches
	sudo free
	$1 ./mage $2 $3 $4 $WORKER_ID $5 > ~/work/logs/${5}_${6}_${7}.log
	if [ $ROLE == "garbler" ]
	then
		diff ${5}_${WORKER_ID}.output ${5}_${WORKER_ID}.expected > ~/work/logs/${5}_${6}_${7}.result
	fi
}

function run_benchmark {
	# $1 is protocol (e.g., halfgates)
	# $2 is the problem name
	# $3 is the problem size
	# $4 is the number of trials
	# $5 is the regular config file
	# $6 is the unbounded config file
	# $7 is a boolean specifying whether to generate new input each time
	# $8 is a boolean specifying whether to sleep between iterations
	for trial in $(seq $4)
	do
		sudo swapoff -a
		if [[ $7 = true ]]
		then
			./example_input $2 $3 $NUM_WORKERS
		fi
		./planner $2 $6 $ROLE $WORKER_ID $3
		sudo swapon ${SWAP_DEVICE}
		invoke "sudo cgexec -g memory:memprog1gb" $1 $6 $ROLE ${2}_${3} os t$trial $8
		sudo swapoff -a
		invoke "sudo" $1 $6 $ROLE ${2}_${3} unbounded t$trial $8
		./planner $2 $5 $ROLE $WORKER_ID $3
		invoke "sudo cgexec -g memory:memprog1gb" $1 $5 $ROLE ${2}_${3} mage t$trial $8
	done
}

mkdir -p ~/work/logs

run_benchmark halfgates merge_sorted 1048576 1 ../config.yaml ../config_unbounded.yaml true true
run_benchmark halfgates full_sort 1048576 1 ../config.yaml ../config_unbounded.yaml true true
run_benchmark halfgates loop_join 2048 1 ../config.yaml ../config_unbounded.yaml true true
run_benchmark halfgates matrix_vector_multiply 8192 1 ../config.yaml ../config_unbounded.yaml true true
run_benchmark halfgates binary_fc_layer 16384 1 ../config.yaml ../config_unbounded.yaml true true

run_benchmark ckks real_sum 65536 1 ../config_ckks.yaml ../config_ckks_unbounded.yaml false false
run_benchmark ckks real_statistics 16384 1 ../config_ckks.yaml ../config_ckks_unbounded.yaml false false
run_benchmark ckks real_matrix_vector_multiply 256 1 ../config_ckks.yaml ../config_ckks_unbounded.yaml false false
run_benchmark ckks real_naive_matrix_multiply 128 1 ../config_ckks.yaml ../config_ckks_unbounded.yaml false false
run_benchmark ckks real_tiled_matrix_multiply 128 1 ../config_ckks.yaml ../config_ckks_unbounded.yaml false false
