#!/usr/bin/env python3
"""Compatibility entry point for :mod:`commands.configure`."""

import sys

from commands.configure import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
