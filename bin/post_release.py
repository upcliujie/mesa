#!/usr/bin/env python3
# Copyright © 2019-2020 Intel Corporation

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""Update release notes SHA sums and calendar entries."""

from __future__ import annotations
import argparse
import csv
import pathlib
import subprocess
import typing

if typing.TYPE_CHECKING:

    class Arguments(typing.Protocol):

        version: str


def update_sha_sums(version: str) -> None:
    p = pathlib.Path(__file__).parent.parent / 'docs' / 'relnotes' / f'{version}.rst'
    with p.open('r') as f:
        notes = f.read()

    # Since the script calling this will use builddir, this should be safe
    with (pathlib.Path(__file__).parent.parent / 'builddir' / 'meson-dist' / f'mesa-{version}.announce').open('r') as f:
        for line in f.readlines():
            if line.startswith('SHA256'):
                sha = line[len('SHA256: '):]
                break
        else:
            raise RuntimeError('could not find SHA256 sum in announce')

    with p.open('w') as f:
        f.write(notes.replace('TBD.', sha))

    subprocess.run(['git', 'add', p])
    subprocess.run(['git', 'commit', '-m',
                    f'docs: Add sha256 sum for {version}'])


def update_calendar(version: str) -> None:
    p = pathlib.Path('docs') / 'release-calendar.csv'

    with p.open('r') as f:
        calendar = list(csv.reader(f))

    branch = None
    for i, line in enumerate(calendar):
        if line[2] == version:
            if line[0]:
                branch = line[0]
            break
    if branch is not None:
        calendar[i + 1][0] = branch
    del calendar[i]

    with p.open('w') as f:
        writer = csv.writer(f)
        writer.writerows(calendar)

    subprocess.run(['git', 'add', p])
    subprocess.run(['git', 'commit', '-m',
                    f'docs: Update release calendar for {version}'])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('version', help="the released version")
    args: Arguments = parser.parse_args()

    # We have nothing  to do in this case, since we didn't generate notes
    if '-rc' in args.version:
        return

    update_sha_sums(args.version)
    update_calendar(args.version)

    subprocess.run(['git', 'push'])


if __name__ == "__main__":
    main()
