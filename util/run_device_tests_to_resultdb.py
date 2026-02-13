#!/usr/bin/env python3
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Upload run_device_tests.py results to ResultDB

Usage:
$ rdb stream -new -realm chromium:public -tag builder_name:${HOSTNAME%%.*}
  -- ./util/run_device_tests_to_resultdb.py --results=test_results.json
  --upload
"""

import argparse
import base64
import json
import os
import requests  # pylint: disable=import-error


def testcase_to_result(testcase):
    """Translates Renode testcase to ResultDB format"""
    result = {
        "testId": testcase["test_name"],
        "status": testcase["status"],
        "expected": testcase["status"] == "PASS",
        "summaryHtml": '<p><text-artifact artifact-id="test_log"></p>',
        "artifacts": {
            "test_log": {
                "contents": base64.b64encode(testcase["logs"].encode()).decode(),
            },
        },
        "tags": [
            {"key": "platform", "value": "renode"},
        ],
    }
    return result


def json_to_resultdb(result_file):
    """Translates Renode JSON test report to ResultDB format"""
    with open(result_file, encoding="utf-8") as file:
        data = json.load(file)
        results = []
        for testcase in data.get("tests", []):
            results.append(testcase_to_result(testcase))
    return results


class BytesEncoder(json.JSONEncoder):
    """Encoder for ResultDB format"""

    def default(self, o):
        if isinstance(o, bytes):
            return o.decode("utf-8")
        return json.JSONEncoder.default(self, o)


def upload_results(results):
    """Upload results to ResultDB"""
    if "LUCI_CONTEXT" not in os.environ:
        print("LUCI_CONTEXT not found. Skipping upload.")
        return

    with open(os.environ["LUCI_CONTEXT"], encoding="utf-8") as file:
        sink = json.load(file).get("result_sink")
        if not sink:
            print("result_sink not found in LUCI_CONTEXT. Skipping upload.")
            return

    # Uploads all test results at once.
    res = requests.post(
        url=f"http://{sink['address']}/prpc/luci.resultsink.v1.Sink/ReportTestResults",
        headers={
            "Content-Type": "application/json",
            "Accept": "application/json",
            "Authorization": f"ResultSink {sink['auth_token']}",
        },
        data=json.dumps({"testResults": results}, cls=BytesEncoder),
    )
    res.raise_for_status()


def main():
    """main"""
    parser = argparse.ArgumentParser(
        description=("Upload run_device_tests.py results to ResultDB")
    )
    parser.add_argument("--results", required=True)
    parser.add_argument("--upload", default=False, action="store_true")
    args = parser.parse_args()

    if args.results:
        print(f"Converting: {args.results}")
        try:
            rdb_results = json_to_resultdb(args.results)
            if args.upload:
                upload_results(rdb_results)
            else:
                print(json.dumps(rdb_results, indent=2, cls=BytesEncoder))
        except Exception as e:
            print(f"Error processing results: {e}")
            # Do not fail the build if upload fails, just log it.
            pass


if __name__ == "__main__":
    main()
