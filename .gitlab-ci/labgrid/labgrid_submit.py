#!/usr/bin/env python3
#
# Copyright © David Heidelberg
# SPDX-License-Identifier: MIT
"""Send job to Labgrid while providing status and job run result."""

import sys
import argparse
import labgrid


def parse_arguments():
    """Parse input arguments"""
    parser = argparse.ArgumentParser(description="Script to execute commands with a specified timeout.")
    parser.add_argument("--job-timeout", type=int, default=30, help="Timeout for command execution in minutes.")
    return parser.parse_args()


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

    result: list[str] = []
    try:
        result = command.run_check('/init-stage2.sh', timeout=args.job_timeout * 60)
    except labgrid.driver.exception.ExecutionError:
        print("init-stage2.sh failed")

    strategy.transition("off")

    if 'hwci: mesa: pass' in result:
        sys.exit(0)
    elif 'hwci: mesa: fail' in result:
        sys.exit(1)
    else:
        sys.exit(255)


if __name__ == "__main__":
    main()
