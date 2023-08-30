#!/usr/bin/env python3
#
# Copyright © David Heidelberg
# SPDX-License-Identifier: MIT
"""Send job to Labgrid while providing status and job run result."""

import sys
import os
import time
import threading
import argparse
import labgrid

result: list[str] = []


def is_job_hanging(max_idle_time: int):
    """Periodically check if the file changed, if not exit the script"""
    current_time: int = time.time()

    file_modified_time: int = os.path.getmtime("results/logs/test/console_main")
    time_diff: int = current_time - file_modified_time

    if time_diff > max_idle_time:
        print(f"Test log didn't changed within {time_diff} seconds. Stopping...")
        return True

    return False


def parse_arguments():
    """Parse input arguments"""
    parser = argparse.ArgumentParser(description="Script to execute commands with a specified timeout.")
    parser.add_argument("--job-timeout", type=int, default=30, help="Timeout for command execution in minutes.")
    return parser.parse_args()


def run_stage2(command, timeout: int):
    '''Executes stage2 script'''
    global result
    try:
        result = command.run_check('/init-stage2.sh', timeout=timeout)
    except labgrid.driver.exception.ExecutionError:
        print("init-stage2.sh failed")
        result = []


def main():
    """main function, TODO: split later"""
    args = parse_arguments()

    env = labgrid.Environment("env.yaml")
    target = env.get_target()
    strategy = target.get_driver("Strategy")

    labgrid.ConsoleLoggingReporter.start("results/logs/uboot")
    strategy.transition("uboot")
    labgrid.ConsoleLoggingReporter.stop()
    labgrid.ConsoleLoggingReporter.start("results/logs/linux")
    strategy.transition("shell")
    labgrid.ConsoleLoggingReporter.stop()
    labgrid.ConsoleLoggingReporter.start("results/logs/test")

    command = target.get_active_driver('CommandProtocol')
    try:
        command.run_check('/init-stage1.sh', timeout=120)
    except labgrid.driver.exception.ExecutionError:
        print("init-stage1.sh failed")

    stage2_t = threading.Thread(target=run_stage2,
                                args=(command, args.job_timeout * 60))
    stage2_t.daemon = True
    stage2_t.start()

    max_idle_time: int = 300

    while stage2_t.is_alive():
        if is_job_hanging(max_idle_time):
            break
        time.sleep(5)

    strategy.transition("off")

    if 'hwci: mesa: pass' in result:
        sys.exit(0)
    elif 'hwci: mesa: fail' in result:
        sys.exit(1)
    else:
        sys.exit(255)


if __name__ == "__main__":
    main()
