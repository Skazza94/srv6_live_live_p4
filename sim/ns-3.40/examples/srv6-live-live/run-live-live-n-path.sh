#!/bin/bash

default_bw="100Mbps"
path_bw="100Mbps"
ll_rate="10Mbps"
maxBytes=12500000
path_delay="5us"
congestion_control="TcpCubic"
ll_flows=1
default_buffer="10000p"
path_buffer="10000p"
seed=1
end=40
dump="--dump"
n_paths=4
test_type="live-live"

result_path="$n_paths/$seed"

mkdir -p results/$result_path
../../ns3 run "live-live-n-path --results-path=examples/srv6-live-live/results/$result_path --ll-flows=$ll_flows --default-bw=$default_bw --ll-rate=$ll_rate --path-bw=$path_bw --path-delay=$path_delay --max-bytes=$maxBytes --congestion-control=$congestion_control --default-buffer=$default_buffer --path-buffer=$path_buffer --end=$end --seed=$seed --n-paths=$n_paths --test-type=$test_type $dump" > results/$result_path/log.txt

python3 flowmon_parser.py results/$result_path/flow-monitor/flow_monitor.xml
python3 plot.py results/$result_path/ figures/$result_path
chmod 777 -R results
chmod 777 -R figures