#!/bin/bash

default_bw="100Mbps"
active_bw="100Mbps"
backup_bw="100Mbps"
ll_rate="10Mbps"
active_rate_tcp="10Mbps"
backup_rate_tcp="10Mbps"
maxBytes=12500000
active_rate_udp="2Mbps"
backup_rate_udp="2Mbps"
active_delay="5us"
backup_delay="5us"
congestion_control="TcpCubic"
ll_flows=0
active_flows=201
backup_flows=201
default_buffer="5000p"
active_buffer="5000p"
backup_buffer="5000p"
seed=4555
flow_end=13
end=40
dump=""
random=""
alternate="--alternate"

random_lbl="b"
if [[ $random != "" ]]
then
    random_lbl="r"
fi


result_path="$ll_flows-$active_flows-$backup_flows-$random_lbl/$default_bw-$active_bw-$backup_bw/$ll_rate-$active_rate_tcp-$backup_rate_tcp-$active_rate_udp-$backup_rate_udp/$active_delay-$backup_delay/$congestion_control/$default_buffer-$active_buffer-$backup_buffer/$seed"

mkdir -p results/$result_path
../../ns3 run "no-live-live-experiment --results-path=examples/srv6-live-live/results/$result_path --ll-flows=$ll_flows --active-flows=$active_flows --backup-flows=$backup_flows  --default-bw=$default_bw --ll-rate=$ll_rate --active-bw=$active_bw --active-delay=$active_delay --active-rate-tcp=$active_rate_tcp --active-rate-udp=$active_rate_udp --backup-bw=$backup_bw --backup-delay=$backup_delay --backup-rate-tcp=$backup_rate_tcp --max-bytes=$maxBytes
--backup-rate-udp=$backup_rate_udp --congestion-control=$congestion_control --default-buffer=$default_buffer --active-buffer=$active_buffer --backup-buffer=$backup_buffer --flow-end=$flow_end --end=$end --seed=$seed $random $dump $alternate" > results/$result_path/log.txt

python3 flowmon_parser.py results/$result_path/flow-monitor/flow_monitor.xml
python3 plot.py results/$result_path/ figures/$result_path
chmod 777 -R results
chmod 777 -R figures