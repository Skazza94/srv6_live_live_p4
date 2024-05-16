#!/bin/bash

active_delay=1
backup_delay=1
congestion_control="ns3::TcpVegas"
dump=0
for ll in 1
do
  for backup in 10
  do
    for active in 1
    do
      mkdir -p results/$ll-$active-$backup
      ../../ns3 run "live-live-experiment --results-path=examples/srv6-live-live/results/$ll-$active-$backup --active-flows=$active --backup-flows=$backup --ll-flows=$ll --active-delay=$active_delay --backup-delay=$backup_delay --congestion-control=$congestion_control --dump=$dump" > results/$ll-$active-$backup/log.txt
      mkdir -p parsed-results/$ll-$active-$backup
      cat results/$ll-$active-$backup/log.txt | grep -E "ll\-port\: [1-2]" | cut -d " " -f 7,10,13 > parsed-results/$ll-$active-$backup/log.txt
    done
  done
done


#python3 parse_flow_monitor.py results