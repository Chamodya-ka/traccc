#!/bin/bash

num_proc=1
events=1
while getopts n:e: flag;
do
    case "${flag}" in
        n) num_proc=${OPTARG};;
        e) events=${OPTARG};;
    esac
done
echo "number of processes : $num_proc";
echo "number of events : $events";
export TRACCC_TEST_DATA_DIR='/home/chamodya/myrepos/recent/traccc/data/'
start=$SECONDS
for((i=0;i<num_proc;i++))
do
	skip=$(($i * $events))
	echo "Skipping $skip events"
	../build/bin/traccc_seq_example_cuda --detector_file=tml_detector/trackml-detector.csv --digitization_config_file=tml_detector/default-geometric-config-generic.json --cell_directory=tml_full/ttbar_mu200/  --events=$events --skip=$skip --input-binary &
	rm -f performance_track_seeding.root
done
wait
duration=$(( SECONDS - start ));
echo "Time : $duration";
