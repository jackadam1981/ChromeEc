#!/usr/bin/python3

import argparse
import logging
import os
import pathlib
import queue
import sys


def _parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Verbose Output"
    )
    parser.add_argument(
        "-d",
        "--dt-has",
        action="store_true",
        help="Check for options that depends on a DT_HAS_..._ENABLE symbol.",
    )
    parser.add_argument(
        "KBLOG_FILE",
        nargs="*",
        help="List of kblog files to be analyzed.",
        type=pathlib.Path,
    )

    return parser.parse_args(argv)


def analyze(file):
    in_led = False
    scanned = queue.Queue()

    with open(file, "r") as f:
        print("Analyzing %s" % (file))
        for line in f:
            for word in line.split():
                if word.startswith('K.'):
                    token = word.split('.')
                    if token[1] == 'fa':
                        print("%s: ACK" % (word))
                    else:
                        print("%s: %s is sent %s" % (word, token[1],
                              "(out-of-order)" if scanned.get() != token[1] else ""))
                elif word.startswith('s.'):
                    token = word.split('.')
                    scanned.put(token[1])
                    print("%s: %s is scanned %s" % (word, token[1],
                          "(LED interference)" if in_led else ""))
                elif word.startswith('d.'):
                    token = word.split('.')
                    if token[1] == 'ed':
                        in_led = True
                        print("%s: LED" % (word))
                    elif token[1] == '04' and in_led:
                        in_led = False
                        print("%s: on" % (word))
                    elif token[1] == '00' and in_led:
                        in_led = False
                        print("%s: off" % (word))
                    else:
                        in_led = False
                        print("%s: %s is received" % (word, token[1]))


def main(argv):
    """Main function"""
    args = _parse_args(argv)

    for file in args.KBLOG_FILE:
        analyze(file)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

