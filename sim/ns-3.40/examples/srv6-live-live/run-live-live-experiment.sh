#!/bin/bash

default_bw="50Mbps"
active_bw="25Mbps"
backup_bw="25Mbps"
ll_rate="1Mbps"
active_rate="1Mbps"
backup_rate="1Mbps"
active_delay="0us"
backup_delay="0us"
congestion_control="TcpBbr"
ll_flows=1
active_flows=25
backup_flows=100
default_buffer="10000p"
active_buffer="10000p"
backup_buffer="10000p"
seed=4666
flow_end=10
end=40
dump=""
random="--random"

random_lbl="b"
if [[ $random != "" ]]
then
    random_lbl="r"
fi


result_path="$ll_flows-$active_flows-$backup_flows-$random_lbl/$default_bw-$active_bw-$backup_bw/$ll_rate-$active_rate-$backup_rate/$active_delay-$backup_delay/$congestion_control/$default_buffer-$active_buffer-$backup_buffer/$seed"

mkdir -p results/$result_path
../../ns3 run "live-live-experiment --results-path=examples/srv6-live-live/results/$result_path --ll-flows=$ll_flows --active-flows=$active_flows --backup-flows=$backup_flows  --default-bw=$default_bw --ll-rate=$ll_rate --active-bw=$active_bw --active-delay=$active_delay --active-rate=$active_rate --backup-bw=$backup_bw --backup-delay=$backup_delay --backup-rate=$backup_rate --congestion-control=$congestion_control --default-buffer=$default_buffer --active-buffer=$active_buffer --backup-buffer=$backup_buffer --flow-end=$flow_end --end=$end --seed=$seed $random $dump" > results/$result_path/log.txt

python3 flowmon-parse-results.py results/$result_path/flow-monitor/flow_monitor.xml