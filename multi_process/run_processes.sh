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

for((i=0;i<num_proc;i++))
do
	skip=$(($i * $events))
	echo "Skipping $skip events"
	# run a traccc_seq_example_cuda job
done
