#!/usr/bin/env python3
"""Compatibility entry point for :mod:`dependencies.resolver`."""

import sys

from dependencies.resolver import main


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
