#!/bin/bash

# run n cuda processes for example n in range 1 -> 32
# benchmark cuda mps for increments of 10 events per process for example i in range 1 -> 50
max_proc=2
max_events=150
increment=10
cores=1		# number of cores (sockets)
threads=1	# number of threads per core 
gpu=1
path='../data'
run_cpu=0

while getopts r:c:t:p:e:g:d: flag;
do
    case "${flag}" in
	c) cores=${OPTARG};;
	t) threads=${OPTARG};;
	p) max_proc=${OPTARG};;
	e) max_events=${OPTARG};;
	g) gpu=${OPTARG};;
	d) path=${OPTARG};;
	r) run_cpu=${OPTARG};;
    esac
done

echo "$max_proc $max_events";
for((i=max_events;i<=max_events;i+=increment))
do	
	echo "starting to benchmark with $i processes";
	for((j=1;j<=max_proc;j+=1))
	do
		echo "starting new run with $j events";
		if [ $run_cpu != 0 ];then
				
			./benchmark_cpu.sh -p"$path" -n$j -e$i -c$cores -t$threads	
			result=$?
		else
			./benchmark_cuda.sh -p"$path" -n$j -e$i -c$cores -t$threads -g$gpu
			result=$?
		fi
				
		sleep 1
	done		
done
echo "done sucessfully" # testing workflow
exit $result
