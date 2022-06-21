#!/bin/bash
num_proc=1 	# number of processes expected to run concurrently
events=1 	# number of event each process will compute	
cores=1		# number of cores (sockets)
threads=1	# number of threads per core 
datapath=$1
echo "$path"
while getopts n:e:c:t: flag;
do
    case "${flag}" in
        n) num_proc=${OPTARG};;
        e) events=${OPTARG};;
	c) cores=${OPTARG};;
	t) threads=${OPTARG};;
    esac
done
echo "number of processes : $num_proc";
echo "number of events : $events";
export TRACCC_TEST_DATA_DIR=$datapath'
start=$SECONDS
for((i=0;i<num_proc;i++))
do
	skip=$(($i * $events))
	echo "Skipping $skip events";
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
	taskset -c $p ../build/bin/traccc_cuda_example --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json --cell_directory=tml_full/ttbar_mu200/  --events=$events --skip=$skip --input-binary &
	#cur_pid=$!
	#echo "$(taskset -pc $cur_pid)"
	#echo "currentbg :$cur_pid"
	#rm -f performance_track_seeding.root
done
wait
duration=$(( SECONDS - start ));
echo "Time : $duration";
