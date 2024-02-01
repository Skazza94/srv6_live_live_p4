import ipaddress
import json
import logging
import os.path
import shlex
import shutil
import statistics
import sys
import time
from distutils.dir_util import copy_tree, remove_tree

from Kathara.manager.Kathara import Kathara
from Kathara.parser.netkit.LabParser import LabParser

from colored_logging import set_logging


def get_output(exec_output):
    output = ""
    try:
        while True:
            (stdout, _) = next(exec_output)
            stdout = stdout.decode('utf-8') if stdout else ""

            if stdout:
                output += stdout
    except StopIteration:
        pass
    return output


def run_test(test_folder: str, test_number: int, number_of_flows_active: int, number_of_flows_backup: int):
    lab_path = "lab_multiple_flows"
    test_lab_path = "test_lab"

    if os.path.isdir(test_lab_path):
        remove_tree(test_lab_path)

    logging.info("Creating test lab...")
    copy_tree(lab_path, test_lab_path)

    kathara = Kathara.get_instance()
    kathara.wipe()

    logging.info("Parsing base lab...")
    lab = LabParser.parse(test_lab_path)

    logging.info("Adding congestion...")

    e1 = lab.get_machine("e1")
    e1.create_file_from_path(os.path.join('assets', 'multiple-flows',
                                          'commands', test_type, 'e1.txt'), 'commands.txt')

    e2 = lab.get_machine("e2")
    e2.create_file_from_path(os.path.join('assets', 'multiple-flows',
                                          'commands', test_type, 'e2.txt'), 'commands.txt')

    logging.info("Generating ips for client on backup path...")
    available_client_ips = ipaddress.ip_network("2003::/64").hosts()
    assigned_client_ips = []
    for idx in range(0, number_of_flows_backup):
        ip = next(available_client_ips)
        lab.write_line_after("client.startup", line_to_add=f"ip addr add {ip} dev eth0",
                             searched_line=f"ip addr add 2003::1a/64 dev eth0")
        assigned_client_ips.append(ip)

    logging.info("Generating ips for server on backup path...")
    available_server_ips = ipaddress.ip_network("2004::/64").hosts()
    assigned_server_ips = []
    for idx in range(0, number_of_flows_backup):
        ip = next(available_server_ips)
        lab.write_line_after("server.startup", line_to_add=f"ip addr add {ip} dev eth0",
                             searched_line=f"ip addr add 2004::1b/64 dev eth0")
        assigned_server_ips.append(ip)

    logging.info("Generating ips for client on active path...")
    available_client_ips = ipaddress.ip_network("2005::/64").hosts()
    for idx in range(0, number_of_flows_active):
        ip = next(available_client_ips)
        lab.write_line_after("client.startup", line_to_add=f"ip addr add {ip} dev eth0",
                             searched_line=f"ip addr add 2003::1a/64 dev eth0")
        assigned_client_ips.append(ip)

    logging.info("Generating ips for server on backup path...")
    available_server_ips = ipaddress.ip_network("2006::/64").hosts()
    for idx in range(0, number_of_flows_active):
        ip = next(available_server_ips)
        lab.write_line_after("server.startup", line_to_add=f"ip addr add {ip} dev eth0",
                             searched_line=f"ip addr add 2004::1b/64 dev eth0")
        assigned_server_ips.append(ip)

    lab.write_line_after(
        file_path="e1.startup",
        line_to_add=f"tc qdisc add dev eth3 root tbf rate 10Mbit buffer 2kb latency 10ms",
        searched_line="ip link set eth3 address 00:00:00:e1:c2:00"
    )

    logging.info("Deploying lab...")
    kathara.deploy_lab(lab)
    time.sleep(5)

    logging.info("Dumping e2 iperf server...")

    tcpdump_pid_1 = get_output(kathara.exec(
        machine_name="e2",
        command=shlex.split("/bin/bash -c 'tcpdump -tenni eth0 -w /shared/e20.pcap \"ether[74:4] == 0x55\" & echo $!'"),
        lab_hash=lab.hash
    )).strip()

    tcpdump_pid_2 = get_output(
        kathara.exec(
        machine_name="e2",
        command=shlex.split("/bin/bash -c 'tcpdump -tenni eth1 -w /shared/e21.pcap \"ether[74:4] == 0x55\" & echo $!'"),
        lab_hash=lab.hash
    )).strip()

    logging.info("Launching live-live iperf server...")
    kathara.exec(
        machine_name="b",
        command=shlex.split("/bin/bash -c '/usr/bin/iperf3 -6 -s'"),
        lab_hash=lab.hash
    )

    logging.info("Launching iperf servers...")
    for ip in assigned_server_ips:
        kathara.exec(
            machine_name="server",
            command=shlex.split(f"/bin/bash -c '/usr/bin/iperf3 -6 -s -B {ip}'"),
            lab_hash=lab.hash
        )
    time.sleep(5)

    logging.info("Launching iperf clients...")
    iperf_clients_stats = []
    for idx, ip in enumerate(assigned_client_ips):
        iperf_clients_stats.append(kathara.exec(
            machine_name="client",
            command=shlex.split(f"/bin/bash -c 'iperf3 -6 -c {assigned_server_ips[idx]} -n 10MB -J -B {ip}'"),
            lab_hash=lab.hash
        ))

    logging.info("Launching live-live iperf client...")
    exec_output = kathara.exec(
        machine_name="a",
        command=shlex.split(f"/bin/bash -c 'iperf3 -6 -c 2002::b -n 10MB -J'"),
        lab_hash=lab.hash
    )

    logging.info("Waiting iperf experiment...")
    output = get_output(exec_output)
    iperf_clients_stats = list(map(lambda x: json.loads(get_output(x)), iperf_clients_stats))

    logging.info("Stop dumping interfaces...")
    kathara.exec(
        machine_name="e1",
        command=shlex.split(f"/bin/bash -c 'kill -15 {tcpdump_pid_1}'"),
        lab_hash=lab.hash
    )

    kathara.exec(
        machine_name="e2",
        command=shlex.split(f"/bin/bash -c 'kill -15 {tcpdump_pid_2}'"),
        lab_hash=lab.hash
    )

    concurrent_flows_results = os.path.join(test_folder, "concurrent-flows")
    os.makedirs(concurrent_flows_results, exist_ok=True)
    with open(
            os.path.join(concurrent_flows_results, f"iperf_clients_test_{test_number}.json"), 'w') as test_result:
        test_result.write(json.dumps(iperf_clients_stats))

    ll_flow_result = os.path.join(test_folder, "ll-flow")
    os.makedirs(ll_flow_result, exist_ok=True)
    with open(
            os.path.join(ll_flow_result, f"test_{test_number}.json"), 'w') as test_result:
        test_result.write(output)

    logging.info("Undeploying lab...")
    kathara.undeploy_lab(lab=lab)

    pcaps_dir = os.path.join(test_folder, "pcaps", str(test_number))
    os.makedirs(pcaps_dir, exist_ok=True)

    shutil.move(os.path.join(test_lab_path, "shared", "e20.pcap"), os.path.join(pcaps_dir, "e20.pcap"))

    shutil.move(os.path.join(test_lab_path, "shared", "e21.pcap"), os.path.join(pcaps_dir, "e21.pcap"))

    logging.info("Removing test lab...")
    remove_tree(test_lab_path)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(
            "Usage: experiment.py <result_path> <n_runs>"
        )
        exit(1)

    result_path = os.path.abspath(sys.argv[1])
    n_runs = int(sys.argv[2])

    set_logging()

    for test_type in [
        'live-live',
        'baseline'
    ]:
        test_type_path = os.path.join(result_path, test_type)
        if not os.path.isdir(test_type_path):
            os.makedirs(test_type_path, exist_ok=True)

        logging.info(f"Running {test_type} experiments...")
        for flows in [1, 5, 10, 20, 50, 100]:
            logging.info(f"\t- Concurrent flows: {flows}")
            test_folder = os.path.join(test_type_path, str(flows))
            if not os.path.isdir(test_folder):
                os.mkdir(test_folder)

            for run in range(1, n_runs + 1):
                logging.info(f"Starting run {run}")
                run_test(test_folder, run, int(flows/2), flows)
