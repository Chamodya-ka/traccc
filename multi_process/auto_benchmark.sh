#!/bin/bash

# run n cuda processes for example n in range 1 -> 32
# benchmark cuda mps for increments of 10 events per process for example i in range 1 -> 50
max_proc=2
max_events=10
cores=1		# number of cores (sockets)
threads=1	# number of threads per core 
while getopts c:t:p:e: flag;
do
    case "${flag}" in
	c) cores=${OPTARG};;
	t) threads=${OPTARG};;
	p) max_proc=${OPTARG};;
	e) max_events=${OPTARG};;
    esac
done
echo "$max_proc $max_events";
for((i=1;i<=max_proc;i++))
do	
	echo "starting to benchmark with $i processes";
	for((j=0;j<=max_events;j+=5))
	do
		echo "starting new run with $j events";
		./benchmark_cpu.sh -p ../data -n$i -e$j -c$cores -t$threads
		sleep 1
	done		
done

