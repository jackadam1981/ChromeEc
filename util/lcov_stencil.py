#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Merge lcov files, discarding all lines that are not in the template file.

Given 2 or more lcov files, merge the results only for the lines present in
the template file.
"""

import logging
import re
import sys
from collections import defaultdict
from typing import Dict, Set

from chromite.lib import commandline

EXTRACT_LINE = re.compile(r"^(FN|DA|BRDA):(\d+),")
EXTRACT_FN = re.compile(r"^(FN):(\d+),(\S+)")
EXTRACT_FNDA = re.compile(r"^(FNDA):(\d+),(\S+)")
EXTRACT_DA = re.compile(r"^(DA):(\d+),(\d+)")
EXTRACT_BRDA = re.compile(r"^(BRDA):(\d+),(\d+),(\d+),([-\d]+)")
EXTRACT_COUNT = re.compile(r"^([A-Z]+):(\d+)")


def main(argv=None):
    """Merged lcov files."""
    parser = commandline.ArgumentParser()
    parser.add_argument(
        "--output-file",
        "-o",
        help="destination filename, defaults to stdout",
    )
    parser.add_argument(
        "template_file",
        help="lcov info file to use as template",
    )
    parser.add_argument(
        "lcov_input",
        nargs="+",
        help="lcov info file to merge",
    )
    opts = parser.parse_args(argv)

    output_file = sys.stdout
    if opts.output_file:
        logging.info("Writing output to %s", opts.output_file)
        output_file = open(  # pylint:disable=consider-using-with
            opts.output_file, "w"
        )

    logging.info("Reading template file %s", opts.template_file)
    with output_file, open(opts.template_file, "r") as template_file:
        data_by_path: Dict[str, Set[str]] = defaultdict(set)
        file_name = None
        for line in template_file.readlines():
            line = line.strip()
            if line == "end_of_record":
                file_name = None
            elif (
                line.startswith(  # pylint:disable=too-many-boolean-expressions
                    "TN:"
                )
                or line.startswith("FNDA:")
                or line.startswith("FNF:")
                or line.startswith("FNH:")
                or line.startswith("BRF:")
                or line.startswith("BRH:")
                or line.startswith("LF:")
                or line.startswith("LH:")
            ):
                pass
            elif line.startswith("SF:"):
                file_name = line
            else:
                match = EXTRACT_LINE.match(line)
                if file_name and match:
                    data_by_path[file_name].add(match.group(2))
                else:
                    raise NotImplementedError(line)
        for lcov_input in [opts.template_file] + opts.lcov_input:
            logging.info("Merging file %s", lcov_input)
            with open(lcov_input, "r") as input_file:

                def empty_record():
                    return {
                        "text": "",
                        "function_names": set(),
                    }

                record = empty_record()
                for line in input_file.readlines():
                    line = line.strip()
                    if line == "end_of_record":
                        record["text"] += line + "\n"
                        if record.get("should_write_record", False):
                            output_file.write(record["text"])
                        else:
                            logging.debug("Omitting record %s", record["text"])
                        record = empty_record()
                    elif line.startswith("SF:"):
                        record["file_name"] = line
                        record["text"] += line + "\n"
                    elif line.startswith("TN:"):
                        record["text"] += line + "\n"
                    elif line.startswith("FN:"):
                        match = EXTRACT_FN.match(line)
                        if (
                            match
                            and match.group(2)
                            in data_by_path[record["file_name"]]
                        ):
                            record["text"] += line + "\n"
                            record["functions_found"] = (
                                record.get("functions_found", 0) + 1
                            )
                            record["should_write_record"] = True
                            record["function_names"].add(match.group(3))
                        else:
                            logging.debug("Omitting %s", line)
                    elif line.startswith("FNDA:"):
                        match = EXTRACT_FNDA.match(line)
                        if match and match.group(3) in record["function_names"]:
                            record["text"] += line + "\n"
                            record["should_write_record"] = True
                            if match.group(2) != "0":
                                record["functions_hit"] = (
                                    record.get("functions_hit", 0) + 1
                                )
                        else:
                            logging.debug("Omitting %s", line)
                    elif line.startswith("DA:"):
                        match = EXTRACT_DA.match(line)
                        if (
                            match
                            and match.group(2)
                            in data_by_path[record["file_name"]]
                        ):
                            record["text"] += line + "\n"
                            record["lines_found"] = (
                                record.get("lines_found", 0) + 1
                            )
                            record["should_write_record"] = True
                            if match.group(3) != "0":
                                record["lines_hit"] = (
                                    record.get("lines_hit", 0) + 1
                                )
                        else:
                            logging.debug("Omitting %s", line)
                    elif line.startswith("BRDA:"):
                        match = EXTRACT_BRDA.match(line)
                        if (
                            match
                            and match.group(2)
                            in data_by_path[record["file_name"]]
                        ):
                            record["text"] += line + "\n"
                            record["branches_found"] = (
                                record.get("branches_found", 0) + 1
                            )
                            record["should_write_record"] = True
                            if match.group(4) != "-" and match.group(4) != "0":
                                record["branches_hit"] = (
                                    record.get("branches_hit", 0) + 1
                                )
                        else:
                            logging.debug("Omitting %s", line)
                    elif line.startswith("FNF:"):
                        record["text"] += "FNF:%s\n" % record.get(
                            "functions_found", 0
                        )
                    elif line.startswith("FNH:"):
                        record["text"] += "FNH:%s\n" % record.get(
                            "functions_hit", 0
                        )
                    elif line.startswith("BRF:"):
                        record["text"] += "BRF:%s\n" % record.get(
                            "branches_found", 0
                        )
                    elif line.startswith("BRH:"):
                        record["text"] += "BRH:%s\n" % record.get(
                            "branches_hit", 0
                        )
                    elif line.startswith("LF:"):
                        record["text"] += "LF:%s\n" % record.get(
                            "lines_found", 0
                        )
                    elif line.startswith("LH:"):
                        record["text"] += "LH:%s\n" % record.get("lines_hit", 0)
                    else:
                        logging.debug("record = %s", record)
                        raise NotImplementedError(line)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
