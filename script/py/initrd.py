#!/usr/bin/env python3
"""Compatibility entry point for :mod:`generators.initrd`."""

import sys

from generators.initrd import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
