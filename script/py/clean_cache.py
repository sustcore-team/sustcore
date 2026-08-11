#!/usr/bin/env python3
"""Compatibility entry point for :mod:`commands.clean_cache`."""

import sys

from commands.clean_cache import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
