import glob
import logging
import math
import os.path
import os.path
import shlex
import sys
import time

import numpy as np
from Kathara.manager.Kathara import Kathara
from Kathara.model.Lab import Lab

from colored_logging import set_logging


def run_test(lab: Lab, n_path: int, test_folder: str, test_number: int):
    kathara = Kathara.get_instance()
    kathara.wipe()

    logging.info("Adding congestion...")

    delay = 40
    np.random.seed(10)
    loss_dist = np.random.lognormal(1, 0.7, size=n_path)

    for i in range(0, n_path):
        int_loss = math.ceil(loss_dist[i])

        lab.write_line_before(
            file_path="e1.startup",
            line_to_add=f"tc qdisc add dev eth{i} root netem loss {int_loss}% delay {delay}ms",
            searched_line="make all"
        )

    logging.info("Deploying lab...")
    kathara.deploy_lab(lab)

    time.sleep(5)

    logging.info("Launching iperf...")
    kathara.exec(
        machine_name="b",
        command=shlex.split("/bin/bash -c '/usr/bin/iperf3 -6 -s'"),
        lab_hash=lab.hash
    )

    exec_output = kathara.exec(
        machine_name="a",
        command=shlex.split(f"/bin/bash -c 'iperf3 -6 -c 2002::b -b 10M -J'"),
        lab_hash=lab.hash
    )

    logging.info("Waiting iperf experiment...")
    output = ""
    try:
        while True:
            (stdout, _) = next(exec_output)
            stdout = stdout.decode('utf-8') if stdout else ""

            if stdout:
                output += stdout
    except StopIteration:
        pass

    with open(os.path.join(test_folder, f"test_{test_number}.json"), 'w') as test_result:
        test_result.write(output)

    logging.info("Undeploying lab...")
    kathara.undeploy_lab(lab=lab)


def copy_folder_in_device(device, folder):
    for item in glob.glob(os.path.join(folder, "**"), recursive=True):
        c_dev_path = item.replace(folder, "/src/p4src")
        if os.path.isfile(item):
            device.create_file_from_path(item, c_dev_path)


def build_lab(n_paths, test_type):
    template_path = os.path.abspath(os.path.join(".", "assets", "n_path"))
    p4_src_path = os.path.join(template_path, "p4src")
    with open(os.path.join(template_path, "c_dev_template.startup"), "r") as startup_template:
        c_dev_startup_template = startup_template.read()
    with open(os.path.join(template_path, "e_dev_template.startup"), "r") as startup_template:
        e_dev_startup_template = startup_template.read()

    lab = Lab(name=f"paths_{n_paths}")
    a = lab.new_machine(
        "a",
        ipv6=True,
        image="kathara/base",
        sysctls=["net.ipv4.tcp_no_metrics_save=1", "net.ipv4.tcp_rmem=33554432", "net.ipv4.tcp_wmem=33554432"]
    )
    lab.connect_machine_obj_to_link(a, "A")
    a_startup = ("ip link set eth0 address 00:00:00:0a:e1:00 mtu 1400\n" +
                 "ip addr add 2001::a/64 dev eth0\n" +
                 "ip route add default via 2001::e1 dev eth0\n" +
                 "ip -6 neigh add 2001::e1 lladdr 00:00:00:e1:0a:00 dev eth0")
    lab.create_file_from_string(a_startup, "a.startup")

    b = lab.new_machine(
        "b",
        ipv6=True,
        image="kathara/base",
        sysctls=["net.ipv4.tcp_no_metrics_save=1", "net.ipv4.tcp_rmem=33554432", "net.ipv4.tcp_wmem=33554432"]
    )
    lab.connect_machine_obj_to_link(b, "B")
    b_startup = ("ip link set eth0 address 00:00:00:0b:e2:00 mtu 1400\n" +
                 "ip addr add 2002::b/64 dev eth0\n" +
                 "ip route add default via 2002::e2 dev eth0\n" +
                 "ip -6 neigh add 2002::e2 lladdr 00:00:00:e2:0b:00 dev eth0")
    lab.create_file_from_string(b_startup, "b.startup")

    c_devices = {}

    # Create e1
    e1 = lab.new_machine("e1", image="kathara/p4", ipv6=False)
    lab.connect_machine_obj_to_link(e1, "A")  # Connected to Machine "a"
    copy_folder_in_device(e1, p4_src_path)
    e1.create_file_from_path(os.path.join(template_path, "e_dev_Makefile_template"), "Makefile")
    e1_ip_link = ["ip link set eth0 address 00:00:00:e1:0a:00"]
    e1_simple_switch_ifaces = ["-i 1@eth0"]
    for i in range(1, n_paths + 1):
        c_dev_name = f"c{i}"
        c_dev = lab.new_machine(c_dev_name, image="kathara/p4", ipv6=False)
        c_dev_cd_name = f"C{i}E1"
        lab.connect_machine_obj_to_link(c_dev, c_dev_cd_name)
        lab.connect_machine_obj_to_link(e1, c_dev_cd_name)

        copy_folder_in_device(c_dev, p4_src_path)
        c_dev.create_file_from_path(os.path.join(template_path, "c_dev_Makefile_template"), "Makefile")
        startup = c_dev_startup_template.format(eth0_mac=f"00:00:00:c{i}:e1:00", eth1_mac=f"00:00:00:c{i}:e2:00")
        lab.create_file_from_string(startup, f"c{i}.startup")

        if test_type in ["live-live", "no-deduplicate"]:
            commands = [
                "table_add srv6_table srv6_noop 2002::b/128 => 2",
                "table_add srv6_table srv6_noop 2001::a/128 => 1"
            ]
        else:
            commands = [
                "table_add srv6_table srv6_seg_ep e2::2/128 => 2",
                "table_add srv6_table srv6_seg_ep e1::2/128 => 1"
            ]
        c_dev.create_file_from_list(commands, "commands.txt")

        e1_ip_link.append(f"ip link set eth{i} address 00:00:00:e1:c{i}:00")
        e1_simple_switch_ifaces.append(f"-i {i + 1}@eth{i}")

        c_devices[c_dev_name] = c_dev

    e1_commands = ""
    if test_type == "live-live":
        e1_commands = (
                "mc_mgrp_create 1\n" +
                "mc_node_create 1 " + " ".join([str(x) for x in range(2, 2 + n_paths)]) + "\n" +
                "mc_node_associate 1 0\n" +
                "table_add check_live_live_enabled live_live_mcast 2001::/64 => 1 e1::2\n" +
                f"table_set_default check_live_live_enabled ipv6_encap_forward_random e1::2 2 {2 + n_paths}\n" +
                "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e2::55\n" +
                "table_add srv6_function srv6_ll_deduplicate 85 => \n" +
                "table_add ipv6_forward forward 2001::/64 => 1 0x0000000ae100"
        )
    elif test_type == "no-deduplicate":
        e1_commands = (
                "mc_mgrp_create 1\n" +
                "mc_node_create 1 " + " ".join([str(x) for x in range(2, 2 + n_paths)]) + "\n" +
                "mc_node_associate 1 0\n" +
                "table_add check_live_live_enabled live_live_mcast 2001::/64 => 1 e1::2\n" +
                f"table_set_default check_live_live_enabled ipv6_encap_forward_random e1::2 2 {2 + n_paths}\n" +
                "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e2::55\n" +
                "table_add ipv6_forward forward 2001::/64 => 1 0x0000000ae100"
        )
    elif test_type == "random":
        e1_commands = (
                f"table_add check_live_live_enabled ipv6_encap_forward_random 2001::/64 => e1::2 2 {2 + n_paths}\n" +
                "table_add ipv6_forward forward 2001::/64 => 1 0x0000000ae100\n"
        )

        for i in range(2, 2 + n_paths):
            e1_commands += f"table_add srv6_forward add_srv6_dest_segment {i} => e2::2\n"
    elif test_type == "single":
        e1_commands = (
                "table_add check_live_live_enabled ipv6_encap_forward_port 2001::/64 => e1::2 2\n" +
                "table_add srv6_forward add_srv6_dest_segment 2 => e2::2\n" +
                "table_add ipv6_forward forward 2001::/64 => 1 0x0000000ae100"
        )

    e1.create_file_from_string(e1_commands, "commands.txt")
    lab.create_file_from_string(
        e_dev_startup_template.format(
            ip_link="\n".join(e1_ip_link),
            simple_switch_ifaces=" ".join(e1_simple_switch_ifaces)
        ),
        "e1.startup"
    )

    e2 = lab.new_machine("e2", image="kathara/p4", ipv6=False)
    copy_folder_in_device(e2, p4_src_path)
    e2.create_file_from_path(os.path.join(template_path, "e_dev_Makefile_template"), "Makefile")
    e2_ip_link = []
    e2_simple_switch_ifaces = []
    iface_idx = 0
    for c_dev_name, c_dev in c_devices.items():
        c_dev_cd_name = c_dev.name.upper() + "E2"
        lab.connect_machine_obj_to_link(e2, c_dev_cd_name)
        lab.connect_machine_obj_to_link(c_dev, c_dev_cd_name)
        e2_ip_link.append(f"ip link set eth{iface_idx} address 00:00:00:e2:{c_dev.name}:00")
        e2_simple_switch_ifaces.append(f"-i {iface_idx + 1}@eth{iface_idx}")
        iface_idx += 1

    e2_ip_link.append(f"ip link set eth{iface_idx} address 00:00:00:e1:0b:00")
    e2_simple_switch_ifaces.append(f"-i {iface_idx + 1}@eth{iface_idx}")
    lab.create_file_from_string(
        e_dev_startup_template.format(
            ip_link="\n".join(e2_ip_link),
            simple_switch_ifaces=" ".join(e2_simple_switch_ifaces)
        ),
        "e2.startup"
    )
    e2_commands = ""
    if test_type == "live-live":
        e2_commands = (
                "table_add srv6_function srv6_ll_deduplicate 85 => \n" +
                f"table_add ipv6_forward forward 2002::/64 => {iface_idx + 1} 0x0000000be200\n" +
                "mc_mgrp_create 1\n" +
                "mc_node_create 1 " + " ".join([str(x) for x in range(1, 1 + n_paths)]) + "\n" +
                "mc_node_associate 1 0\n" +
                "table_add check_live_live_enabled live_live_mcast 2002::/64 => 1 e2::2\n" +
                f"table_set_default check_live_live_enabled ipv6_encap_forward_random e2::2 1 {1 + n_paths}\n" +
                "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e1::55"
        )
    elif test_type == "no-deduplicate":
        e2_commands = (
                f"table_add ipv6_forward forward 2002::/64 => {iface_idx + 1} 0x0000000be200\n" +
                "mc_mgrp_create 1\n" +
                "mc_node_create 1 " + " ".join([str(x) for x in range(1, 1 + n_paths)]) + "\n" +
                "mc_node_associate 1 0\n" +
                "table_add check_live_live_enabled live_live_mcast 2002::/64 => 1 e2::2\n" +
                f"table_set_default check_live_live_enabled ipv6_encap_forward_random e2::2 1 {1 + n_paths}\n" +
                "table_add srv6_live_live_forward add_srv6_ll_segment 1 => e1::55"
        )
    elif test_type == "random":
        e2_commands = (
                f"table_add check_live_live_enabled ipv6_encap_forward_random 2002::/64 => e2::2 1 {1 + n_paths}\n" +
                f"table_add ipv6_forward forward 2002::/64 => {iface_idx + 1} 0x0000000be200\n"
        )

        for i in range(1, 1 + n_paths):
            e2_commands += f"table_add srv6_forward add_srv6_dest_segment {i} => e1::2\n"
    elif test_type == "single":
        e2_commands = (
                "table_add check_live_live_enabled ipv6_encap_forward_port 2002::/64 => e2::2 1\n" +
                f"table_add srv6_forward add_srv6_dest_segment 1 => e1::2\n" +
                f"table_add ipv6_forward forward 2002::/64 => {iface_idx + 1} 0x0000000be200"
        )

    e2.create_file_from_string(e2_commands, "commands.txt")
    lab.connect_machine_obj_to_link(e2, "B")  # Connected to Machine "b"

    return lab


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(
            "Usage: experiment_n_path.py <result_path> <max_path> <n_runs>"
        )
        exit(1)

    result_path = os.path.abspath(sys.argv[1])
    max_path = int(sys.argv[2])
    n_runs = int(sys.argv[3])

    set_logging()

    for n_path in range(2, max_path + 1):
        for test_type in ['live-live', 'random', 'single', 'no-deduplicate']:
            test_type_path = os.path.join(result_path, test_type, str(n_path))
            if not os.path.isdir(test_type_path):
                os.makedirs(test_type_path, exist_ok=True)

            logging.info(f"Running {test_type} experiments...")
            logging.info(f"Building lab with {n_path} paths")

            lab = build_lab(n_path, test_type)

            for run in range(1, n_runs + 1):
                logging.info(f"Starting run {run}")
                run_test(lab, n_path, test_type_path, run)
