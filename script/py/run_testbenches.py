#!/usr/bin/env python3
"""Compatibility entry point for :mod:`commands.run_testbenches`."""

import sys

from commands.run_testbenches import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
