#!/bin/bash

dump=false
default_bw="25Gbps"
active_bw="25Gbps"
backup_bw="25Gbps"
ll_rate="1Gbps"
active_rate="1Gbps"
backup_rate="1Gbps"

for active_delay in 10 20 30 40
do
  for backup_delay in 20 30 40 50
  do
    for congestion_control in "ns3::TcpVegas" "ns3::TcpBbr" "ns3::TcpCubic"
    do
      for ll_flows in 1 5 10
      do
        for backup_flows in 1 5 10
        do
          for active_flows in 1 5 10
          do
            result_path="$ll_flows-$active_flows-$backup_flows/$default_bw-$active_bw-$backup_bw/$ll_rate-$active_rate-$backup_rate/$active_delay-$backup_delay/$congestion_control"

            mkdir -p results/$result_path
            ../../ns3 run "live-live-experiment --results-path=examples/srv6-live-live/results/$result_path --ll-flows=$ll_flows --active-flows=$active_flows --backup-flows=$backup_flows  --default-bw=$default_bw --ll-rate=$ll_rate --active-bw=$active_bw --active-delay=$active_delay --active-rate=$active_rate --backup-bw=$backup_bw --backup-delay=$backup_delay --backup-rate=$backup_rate --congestion-control=$congestion_control --dump=$dump" > results/$result_path/log.txt
            # mkdir -p parsed-results/$ll_flows-$active_flows-$backup_flows
            # cat results/$ll_flows-$active_flows-$backup_flows/log.txt | grep -E "ll\-port\: [1-2]" | cut -d " " -f 7,10,13 > parsed-results/$ll_flows-$active_flows-$backup_flows/log.txt
          done
        done
      done
    done
  done
done

#python3 parse_flow_monitor.py results