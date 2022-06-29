#!/bin/bash

num_proc=1 	# number of processes expected to run concurrently
events=1 	# number of event each process will compute	
cores=1		# number of cores (sockets)
threads=1	# number of threads per core 
datapath=""
numgpus=1;
while getopts n:e:c:t:p:g: flag;
do
    case "${flag}" in
        n) num_proc=${OPTARG};;
        e) events=${OPTARG};;
	c) cores=${OPTARG};;
	t) threads=${OPTARG};;
	p) datapath=${OPTARG};;
	g) numgpus=${OPTARG};;
    esac
done
echo "$datapath"
echo "number of processes : $num_proc";
echo "number of events : $events";
export TRACCC_TEST_DATA_DIR=$datapath
Tstart=$(date "+%s.%3N")

for((i=0;i<num_proc;i++))
do
	# get processor id	
	if [ $threads -eq 2 ];then
		X=$(($i/$cores))
		Y=$(($X % $threads))
		
		if [ $Y -eq 0 ];then
			p=$((($i % $cores)*2))
			
		else
			p=$((($i % $cores)*2+1))
		fi
	fi	

	if [ $threads -eq 1 ];then
		p=$((($i % $cores)))
	fi
	# end get processor id

	# get gpu id
	gpu_id=$(($i % $numgpus))
	echo "gpu $gpu_id";
	# end get gpu id

	CUDA_VISIBLE_DEVICES=$gpu_id taskset -c $p ../build/bin/traccc_cuda_example --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json --cell_directory=tml_full/ttbar_mu200/  --events=$events --input-binary &
done
wait
Tend=$(date "+%s.%3N")
elapsed=$(echo "scale=3; $Tend - $Tstart" | bc)
python3 log_data.py $num_proc $events $elapsed $cores $threads cuda
#echo "Elapsed: $elapsed ms"
