import logging
import os.path
import shlex
import shutil
import time
from distutils.dir_util import copy_tree, remove_tree

from Kathara.manager.Kathara import Kathara
from Kathara.parser.netkit.LabParser import LabParser
from Kathara.setting.Setting import Setting

from colored_logging import set_logging


def run_test(test_type: str, delay: int, test_number: int):
    lab_path = "lab"
    test_lab_path = "test_lab"

    if os.path.isdir(test_lab_path):
        remove_tree(test_lab_path)

    logging.info("Creating test lab...")
    copy_tree(lab_path, test_lab_path)

    Setting.get_instance().load_from_dict({'enable_ipv6': True})

    kathara = Kathara.get_instance()
    kathara.wipe()

    logging.info("Parsing base lab...")
    lab = LabParser.parse(test_lab_path)

    logging.info("Adding congestion...")

    lab.write_line_after(
        file_path="e1.startup",
        line_to_add=f"tc qdisc add dev eth1 root netem loss 10% delay {delay}ms",
        searched_line="ip link set eth2 address 00:00:00:e1:c2:00"
    )

    lab.write_line_after(
        file_path="e1.startup",
        line_to_add=f"tc qdisc add dev eth2 root netem loss 10% delay {delay}ms",
        searched_line=f"tc qdisc add dev eth1 root netem loss 10% delay {delay}ms"
    )

    e1 = lab.get_machine("e1")
    e1.create_file_from_path(os.path.join('assets', 'commands', test_type, 'e1.txt'), 'commands.txt')

    e2 = lab.get_machine("e2")
    e2.create_file_from_path(os.path.join('assets', 'commands', test_type, 'e2.txt'), 'commands.txt')

    logging.info("Deploying lab...")
    kathara.deploy_lab(lab)

    kathara.exec(
        machine_name="b",
        command="iperf3 -6 -s",
        lab_hash=lab.hash
    )

    time.sleep(3)

    exec_output = kathara.exec(
        machine_name="a",
        command=shlex.split(f"iperf3 -6 -c 2002::b --logfile /shared/test_{test_number}.txt"),
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

    shutil.copy(os.path.join(test_lab_path, "shared", f"test_{test_number}.txt"),
                os.path.join("results", test_type, f"{delay}", f"test_{test_number}.txt"))

    logging.info("Undeploying lab...")
    kathara.undeploy_lab(lab=lab)

    logging.info("Removing test lab...")
    remove_tree(test_lab_path)


if __name__ == '__main__':
    set_logging()

    # if not os.path.isdir(os.path.join("results", "live-live")):
    #     os.mkdir(os.path.join("results", "live-live"))
    #
    # logging.info("Running live live experiments...")
    # for delay in range(10, 110, 10):
    #     logging.info(f"\t- DELAY: {delay}")
    #     if not os.path.isdir(os.path.join("results", "live-live", str(delay))):
    #         os.mkdir(os.path.join("results", "live-live", str(delay)))
    #     for run in range(2, 4):
    #         logging.info(f"Starting run {run}")
    #         run_test('live-live', delay, run)

    if not os.path.isdir(os.path.join("results", "baseline")):
        os.mkdir(os.path.join("results", "baseline"))

    logging.info("Running baseline experiments...")
    for delay in range(10, 110, 10):
        logging.info(f"\t- DELAY: {delay}")
        if not os.path.isdir(os.path.join("results", "baseline", str(delay))):
            os.mkdir(os.path.join("results", "baseline", str(delay)))
        for run in range(1, 3):
            logging.info(f"Starting run {run}")
            run_test('baseline', delay, run)
