#!/usr/bin/env python3
#
# Verify the public machine surface of the focused qemu-pi4 build.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import subprocess
import sys


EXPECTED_MACHINES = ['none', 'raspi4b']
if sys.maxsize > 2**32:
    EXPECTED_MACHINES.insert(1, 'raspi400')


def main() -> int:
    if len(sys.argv) != 2:
        print(f'usage: {sys.argv[0]} QEMU-SYSTEM-AARCH64', file=sys.stderr)
        return 2

    result = subprocess.run([sys.argv[1], '-machine', 'help'],
                            check=True, capture_output=True, text=True)
    lines = result.stdout.splitlines()
    machines = [line.split()[0] for line in lines[1:] if line.split()]

    if machines != EXPECTED_MACHINES:
        print('unexpected machine list in focused Pi 4 build:',
              file=sys.stderr)
        print(f'  expected: {" ".join(EXPECTED_MACHINES)}', file=sys.stderr)
        print(f'  actual:   {" ".join(machines)}', file=sys.stderr)
        return 1

    print(f'Pi 4 machine list: {" ".join(machines)}')

    if 'raspi400' in machines:
        invalid_ram = subprocess.run(
            [sys.argv[1], '-machine', 'raspi400', '-m', '2G',
             '-display', 'none', '-serial', 'none'],
            capture_output=True, text=True)
        if (invalid_ram.returncode == 0 or
                'Invalid RAM size, should be 4 GiB' not in invalid_ram.stderr):
            print('raspi400 accepted an invalid RAM size or returned an '
                  'unexpected diagnostic:', file=sys.stderr)
            print(invalid_ram.stderr, file=sys.stderr)
            return 1
        print('Pi 400 fixed RAM size: 4 GiB')

    return 0


if __name__ == '__main__':
    sys.exit(main())
