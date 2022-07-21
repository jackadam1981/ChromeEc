#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Upload twister results to ResultDB
"""

import argparse
import base64
import json
import os
import sys

import requests


def translate_status(status):
    ret_status = "PASS"

    if status == "passed":
        ret_status = "PASS"
    elif status == "failed":
        ret_status = "FAIL"
    elif status == "skipped":
        ret_status = "SKIP"

    return ret_status


def translate_expected(status):
    flag = False

    if status in ["passed", "skipped"]:
        flag = True

    return flag


def testcase_summary(testcase):
    html = "<p>None</p>"

    if "log" in testcase:
        html = (
            '<p><text-artifact artifact-id="artifact-content-in-request"></p>'
        )

    return html


def testcase_artifact(testcase):
    artifact = base64.b64encode("None".encode())

    if "log" in testcase:
        artifact = base64.b64encode(testcase["log"].encode())

    return artifact


def testcase_to_result(testsuite, testcase):
    result = {
        "testId": testcase["identifier"],
        "status": translate_status(testcase["status"]),
        "expected": translate_expected(testcase["status"]),
        "summaryHtml": testcase_summary(testcase),
        "artifacts": {
            "artifact-content-in-request": {
                "contents": testcase_artifact(testcase),
            }
        },
        "tags": [
            {"key": "category", "value": "ChromeOS/EC"},
            {"key": "platform", "value": testsuite["platform"]},
        ],
        "duration": "%sms" % testcase["execution_time"],
        "testMetadata": {"name": testcase["identifier"]},
    }

    return result


def json_to_resultdb(result_file):
    f = open(result_file)
    data = json.load(f)
    results = []

    for testsuite in data["testsuites"]:
        for testcase in testsuite["testcases"]:
            results.append(testcase_to_result(testsuite, testcase))

    f.close()

    return results


class BytesEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, bytes):
            return obj.decode("utf-8")
        return json.JSONEncoder.default(self, obj)


def upload_results(results):
    with open(os.environ["LUCI_CONTEXT"]) as f:
        sink = json.load(f)["result_sink"]

    # Uploads all test results at once.
    res = requests.post(
        url="http://%s/prpc/luci.resultsink.v1.Sink/ReportTestResults"
        % sink["address"],
        headers={
            "Content-Type": "application/json",
            "Accept": "application/json",
            "Authorization": "ResultSink %s" % sink["auth_token"],
        },
        data=json.dumps({"testResults": results}, cls=BytesEncoder),
    )
    res.raise_for_status()


def main(argv):
    # Set up argument parser.
    parser = argparse.ArgumentParser(
        description=("Upload Zephyr Twister test results to ResultDB")
    )
    parser.add_argument("--results")
    parser.add_argument("--upload", default=False)
    args = parser.parse_args()

    if args.results:
        print("Converting:", args.results)
        rdb_results = json_to_resultdb(args.results)
        # print(rdb_results)
        if args.upload:
            upload_results(rdb_results)
    else:
        print("Missing test result file for conversion")


if __name__ == "__main__":
    main(sys.argv)
