#!/bin/bash

default_bw="50Mbps"
active_bw="50Mbps"
backup_bw="50Mbps"
ll_rate="1Mbps"
active_rate="1Mbps"
backup_rate="1Mbps"
active_delay="5us"
backup_delay="5us"
congestion_control="TcpLinuxReno"
ll_flows=1
active_flows=30
backup_flows=80
default_buffer="10000p"
active_buffer="10000p"
backup_buffer="10000p"
seed=4666
flow_end=10
end=40
dump=""
random="--random"



for active_flows in 25 45
do
    for backup_flows in 45 25 
    do
        for congestion_control in "TcpLinuxReno" "TcpBbr" "TcpCubic" "TcpVegas"
        do
            for seed in 10 4666
            do
                for random in "" "--random"
                do
                    random_lbl="b"
                    if [[ $random != "" ]]
                    then
                        random_lbl="r"
                    fi
                    result_path="$ll_flows-$active_flows-$backup_flows-$random_lbl/$default_bw-$active_bw-$backup_bw/$ll_rate-$active_rate-$backup_rate/$active_delay-$backup_delay/$congestion_control/$default_buffer-$active_buffer-$backup_buffer/$seed"

                    mkdir -p results/$result_path
                    ../../ns3 run "live-live-experiment --results-path=examples/srv6-live-live/results/$result_path --ll-flows=$ll_flows --active-flows=$active_flows --backup-flows=$backup_flows  --default-bw=$default_bw --ll-rate=$ll_rate --active-bw=$active_bw --active-delay=$active_delay --active-rate=$active_rate --backup-bw=$backup_bw --backup-delay=$backup_delay --backup-rate=$backup_rate --congestion-control=$congestion_control --default-buffer=$default_buffer --active-buffer=$active_buffer --backup-buffer=$backup_buffer --flow-end=$flow_end --end=$end --seed=$seed $random $dump" > results/$result_path/log.txt

                    python3 flowmon_parser.py results/$result_path/flow-monitor/flow_monitor.xml
                    python3 plot.py results/$result_path/ figures/$result_path
                done
            done
        done
    done
done

chmod 777 -R results
