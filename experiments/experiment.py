import logging
import os.path
import shlex
import sys
import time
from distutils.dir_util import copy_tree, remove_tree

from Kathara.manager.Kathara import Kathara
from Kathara.parser.netkit.LabParser import LabParser

from colored_logging import set_logging


def run_test(test_folder: str, delay: int, loss: float, test_number: int):
    lab_path = "lab"
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

    lab.write_line_after(
        file_path="e1.startup",
        line_to_add=f"tc qdisc add dev eth1 root netem loss {loss}% delay {delay}ms",
        searched_line="ip link set eth2 address 00:00:00:e1:c2:00"
    )

    lab.write_line_after(
        file_path="e1.startup",
        line_to_add=f"tc qdisc add dev eth2 root netem loss {loss}% delay {delay}ms",
        searched_line=f"tc qdisc add dev eth1 root netem loss {loss}% delay {delay}ms"
    )

    e1 = lab.get_machine("e1")
    e1.create_file_from_path(os.path.join('assets', 'commands', test_type, 'e1.txt'), 'commands.txt')

    e2 = lab.get_machine("e2")
    e2.create_file_from_path(os.path.join('assets', 'commands', test_type, 'e2.txt'), 'commands.txt')

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
        command=shlex.split(f"/bin/bash -c 'iperf3 -6 -c 2002::b -b 6M -J'"),
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

    for test_type in ['live-live', 'baseline']:
        test_type_path = os.path.join(result_path, test_type)
        if not os.path.isdir(test_type_path):
            os.makedirs(test_type_path, exist_ok=True)

        logging.info(f"Running {test_type} experiments...")
        delay_path = os.path.join(test_type_path, "delay")
        if not os.path.isdir(delay_path):
            os.mkdir(delay_path)
        for delay in range(10, 110, 10):
            logging.info(f"\t- DELAY: {delay}")
            test_folder = os.path.join(delay_path, str(delay))
            if not os.path.isdir(test_folder):
                os.mkdir(test_folder)

            for run in range(1, n_runs + 1):
                logging.info(f"Starting run {run}")
                run_test(test_folder, delay, 10, run)

        loss_path = os.path.join(test_type_path, "loss")
        if not os.path.isdir(loss_path):
            os.mkdir(loss_path)
        for loss in range(1, 11, 1):
            logging.info(f"\t- LOSS: {loss}%")
            test_folder = os.path.join(loss_path, str(loss))
            if not os.path.isdir(test_folder):
                os.mkdir(test_folder)

            for run in range(1, n_runs + 1):
                logging.info(f"Starting run {run}")
                run_test(test_folder, 100, loss, run)
