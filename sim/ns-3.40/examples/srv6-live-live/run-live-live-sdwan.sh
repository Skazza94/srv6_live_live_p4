#!/bin/bash

default_bw="25Gbps"
active_bw="25Gbps"
backup_bw="25Gbps"
ll_rate="4Mbps"
active_rate="4Mbps"
backup_rate="1Mbps"
active_delay="5us"
backup_delay="5us"
congestion_control="TcpLinuxReno"
active_flows=500
backup_flows=1
active_buffer="50000p"
backup_buffer="10000p"
seed=5000
flow_end=10
end=40
dump=""
random_lbl="b"

for target_latency in "1.0ms" "2.0ms"
do
    result_path="$active_flows-$backup_flows-$random_lbl/$default_bw-$active_bw-$backup_bw/$ll_rate-$active_rate-$backup_rate/$active_delay-$backup_delay/$congestion_control/$active_buffer-$backup_buffer/$seed"

    mkdir -p results-sdwan-$target_latency/$result_path
    ../../ns3 run "live-live-sdwan --results-path=examples/srv6-live-live/results-sdwan-$target_latency/$result_path --active-flows=$active_flows --backup-flows=$backup_flows  --default-bw=$default_bw --ll-rate=$ll_rate --active-bw=$active_bw --active-delay=$active_delay --active-rate=$active_rate --backup-bw=$backup_bw --backup-delay=$backup_delay --backup-rate=$backup_rate --congestion-control=$congestion_control --active-buffer=$active_buffer --backup-buffer=$backup_buffer --flow-end=$flow_end --end=$end --seed=$seed --sdwan-target-delay=$target_latency $dump" > results-sdwan-$target_latency/$result_path/log.txt
done

mkdir -p results-sdwan-tcp/$result_path
../../ns3 run "live-live-sdwan --results-path=examples/srv6-live-live/results-sdwan-tcp/$result_path --active-flows=$active_flows --backup-flows=$backup_flows  --default-bw=$default_bw --ll-rate=$ll_rate --active-bw=$active_bw --active-delay=$active_delay --active-rate=$active_rate --backup-bw=$backup_bw --backup-delay=$backup_delay --backup-rate=$backup_rate --congestion-control=$congestion_control --active-buffer=$active_buffer --backup-buffer=$backup_buffer --flow-end=$flow_end --end=$end --seed=$seed --sdwan-mode=tcp $dump" > results-sdwan-tcp/$result_path/log.txt
chmod 777 -R results
chmod 777 -R figures