#!/bin/bash

default_bw="100Mbps"
active_bw="100Mbps"
backup_bw="100Mbps"
ll_rate="10Mbps"
active_rate_tcp="10Mbps"
backup_rate_tcp="10Mbps"
maxBytes=20000000
active_rate_udp="1Mbps"
backup_rate_udp="1Mbps"
active_delay="5us"
backup_delay="5us"
congestion_control="TcpLinuxReno"
ll_flows=1
active_flows=1
backup_flows=111
default_buffer="10000p"
active_buffer="10000p"
backup_buffer="1000000p"
seed=23
flow_end=21
end=40
dump=""
random=""
alternate=""

random_lbl="b"
if [[ $random != "" ]]
then
    random_lbl="r"
fi


result_path="$ll_flows-$active_flows-$backup_flows-$random_lbl/$default_bw-$active_bw-$backup_bw/$ll_rate-$active_rate_tcp-$backup_rate_tcp-$active_rate_udp-$backup_rate_udp/$active_delay-$backup_delay/$congestion_control/$default_buffer-$active_buffer-$backup_buffer/$seed"

mkdir -p results/$result_path
../../ns3 run "live-live-experiment --results-path=examples/srv6-live-live/results/$result_path --ll-flows=$ll_flows --active-flows=$active_flows --backup-flows=$backup_flows  --default-bw=$default_bw --ll-rate=$ll_rate --active-bw=$active_bw --active-delay=$active_delay --active-rate-tcp=$active_rate_tcp --active-rate-udp=$active_rate_udp --backup-bw=$backup_bw --backup-delay=$backup_delay --backup-rate-tcp=$backup_rate_tcp --max-bytes=$maxBytes 
--backup-rate-udp=$backup_rate_udp --congestion-control=$congestion_control --default-buffer=$default_buffer --active-buffer=$active_buffer --backup-buffer=$backup_buffer --flow-end=$flow_end --end=$end --seed=$seed $random $dump $alternate" > results/$result_path/log.txt

python3 flowmon_parser.py results/$result_path/flow-monitor/flow_monitor.xml
python3 plot.py results/$result_path/ figures/$result_path
chmod 777 -R results
chmod 777 -R figures