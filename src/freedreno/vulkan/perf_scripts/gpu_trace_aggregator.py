#
# Copyright © 2022 Igalia S.L.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

import re
from typing import Optional
from dataclasses import dataclass
import numpy as np


def calc_mean_and_error(durations):
    mean = np.mean(durations)
    per_90 = np.percentile(durations, 90, interpolation='linear')
    error = (per_90 - mean) / mean * 100.0

    return mean, error


@dataclass
class GPUEvent:
    start: int
    end: int
    duration: int
    name: str
    description: str


@dataclass
class DurationAggregate:
    durations: list[int]
    mean: float = 0.0
    error: float = 0.0

    def update(self):
        self.mean, self.error = calc_mean_and_error(self.durations)


@dataclass
class GPUEventAggregate:
    duration: DurationAggregate
    name: str
    description: str


@dataclass
class AggregateRun:
    name: str
    events: list[GPUEventAggregate]
    total_times: DurationAggregate
    rp_total_times: DurationAggregate


EVENTS_OF_INTEREST = ["render_pass"]

gpu_trace_line = re.compile(".*([0-9]{16})\s+[+-][0-9]+: ([a-z_]+)(.*)")


def gpu_trace_parse_line(event_to_start, line) -> Optional[GPUEvent]:
    match = gpu_trace_line.match(line)
    if not match:
        return

    full_name = match.group(2)

    start_event = full_name.startswith("start_")
    end_event = full_name.startswith("end_")

    if not start_event and not end_event:
        return

    event_name = full_name.split("_", 1)[1]

    if event_name not in EVENTS_OF_INTEREST:
        return

    timestamp = int(match.group(1))

    if end_event:
        desc = match.group(3)
        start = event_to_start[event_name]
        duration = timestamp - start
        assert (duration >= 0)

        del event_to_start[event_name]

        return GPUEvent(start, timestamp, duration, event_name, desc)
    else:
        event_to_start[event_name] = timestamp

    return None


def gpu_trace_parse_file(infile, run_delimiter) -> AggregateRun:
    event_to_start = dict()
    current_run_gpu_events = []
    all_runs = []

    for line in infile.readlines():
        if line.startswith(run_delimiter):
            all_runs.append(current_run_gpu_events)
            current_run_gpu_events = []
        else:
            gpu_event = gpu_trace_parse_line(event_to_start, line)
            if gpu_event:
                current_run_gpu_events.append(gpu_event)

    for run in all_runs:
        assert len(run) == len(all_runs[0])

    aggregate_run = AggregateRun(infile.name, [], DurationAggregate([]), DurationAggregate([]))

    for run in all_runs:
        rp_total_duration = 0
        for event_idx, event in enumerate(run):
            if event_idx >= len(aggregate_run.events):
                duration = DurationAggregate([event.duration])
                aggregate_run.events.append(GPUEventAggregate(duration, event.name, event.description))
            else:
                assert aggregate_run.events[event_idx].name == event.name
                aggregate_run.events[event_idx].duration.durations.append(event.duration)

            rp_total_duration += event.duration

        aggregate_run.rp_total_times.durations.append(rp_total_duration)
        aggregate_run.total_times.durations.append(run[-1].end - run[0].start)

    for event in aggregate_run.events:
        event.duration.update()

    aggregate_run.rp_total_times.update()
    aggregate_run.total_times.update()

    return aggregate_run

