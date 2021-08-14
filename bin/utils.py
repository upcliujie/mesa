# SPDX-License-Identifier: MIT
# Copyright © 2021 Intel Corporation

import asyncio
import json
import pathlib


async def project_version() -> str:
    """Run `meson introspect` to get the project version."""
    v = pathlib.Path(__file__).parent.parent / 'meson.build'
    p = await asyncio.create_subprocess_exec(
        'meson', 'introspect', '--projectinfo', v.as_posix(),
        stdout=asyncio.subprocess.PIPE)
    payload, *_ = await p.communicate()
    assert p.returncode == 0, 'failed to get the project version'
    return json.loads(payload)['version']
