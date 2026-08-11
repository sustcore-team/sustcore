#!/usr/bin/env python3
"""Compatibility entry point for :mod:`commands.clean_build`."""

import sys

from commands.clean_build import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
