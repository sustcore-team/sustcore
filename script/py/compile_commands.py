#!/usr/bin/env python3
"""Compatibility entry point for :mod:`commands.compile_commands`."""

import sys

from commands.compile_commands import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
