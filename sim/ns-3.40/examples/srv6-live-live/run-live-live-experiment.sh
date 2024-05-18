#!/bin/bash

dump="false"
default_bw="25Gbps"
active_bw="25Gbps"
backup_bw="25Gbps"
ll_rate="1Gbps"
active_rate="1Gbps"
backup_rate="1Gbps"
active_delay=1
backup_delay=1
congestion_control="ns3::TcpLinuxReno"
ll_flows=1
backup_flows=100
active_flows=100


result_path="$ll_flows-$active_flows-$backup_flows/$default_bw-$active_bw-$backup_bw/$ll_rate-$active_rate-$backup_rate/$active_delay-$backup_delay/$congestion_control"

mkdir -p results/$result_path
../../ns3 run "live-live-experiment --results-path=examples/srv6-live-live/results/$result_path --ll-flows=$ll_flows --active-flows=$active_flows --backup-flows=$backup_flows  --default-bw=$default_bw --ll-rate=$ll_rate --active-bw=$active_bw --active-delay=$active_delay --active-rate=$active_rate --backup-bw=$backup_bw --backup-delay=$backup_delay --backup-rate=$backup_rate --congestion-control=$congestion_control --dump=$dump" > results/$result_path/log.txt
