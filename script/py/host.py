#!/usr/bin/env python3
"""Compatibility entry point for :mod:`commands.host`."""

import sys

from commands.host import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
