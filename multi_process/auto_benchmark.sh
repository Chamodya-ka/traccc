#!/bin/bash

# run n cuda processes for example n in range 1 -> 32
# benchmark cuda mps for increments of 10 events per process for example i in range 1 -> 50
max_proc=2
max_events=10
cores=1		# number of cores (sockets)
threads=1	# number of threads per core 
gpu=1
while getopts c:t:p:e:g: flag;
do
    case "${flag}" in
	c) cores=${OPTARG};;
	t) threads=${OPTARG};;
	p) max_proc=${OPTARG};;
	e) max_events=${OPTARG};;
	g) gpu=${OPTARG};;

    esac
done
echo "$max_proc $max_events";
for((i=0;i<=max_events;i+=5))
do	
	echo "starting to benchmark with $i processes";
	for((j=1;j<=max_proc;j++))
	do
		echo "starting new run with $j events";
		./benchmark_cuda.sh -p ../data -n$j -e$i -c$cores -t$threads -g$gpu
		sleep 1
	done		
done

