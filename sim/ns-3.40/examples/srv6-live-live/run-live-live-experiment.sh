#!/bin/bash

for ll in 1
do
  for backup in 10 20 50 100
  do
    for active in 10
    do
      mkdir -p results/$ll-$active-$backup
      ../../ns3 run "live-live-experiment --results-path=examples/srv6-live-live/results/$ll-$active-$backup --active-flows=10 --backup-flows=$backup --ll-flows=1" &> results/$ll-$active-$backup/log.txt
      mkdir -p parsed-results/$ll-$active-$backup
      cat results/$ll-$active-$backup/log.txt | grep -E "ll\-port\: [1-2]" &> parsed-results/$ll-$active-$backup/log.txt
    done
  done
done


python3 parse_flow_monitor.py results