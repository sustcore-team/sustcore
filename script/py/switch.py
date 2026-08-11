#!/usr/bin/env python3
"""Compatibility entry point for :mod:`commands.switch`."""

import sys

from commands.switch import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
