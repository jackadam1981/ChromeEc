#!/usr/bin/env python3
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This Tool is for the extractions of HWID and serial no.

Quick start:
  sudo python hwid_extractor.py
"""

from http import server
from urllib import parse as urlparse
import argparse
import json
import logging
import traceback
import os
import sys

from hwid_extractor import device

WWW_ROOT_DIR = os.path.join(os.path.dirname(__file__), 'www')


class StopHandleRequest(BaseException):
    pass


class RequestHandler(server.SimpleHTTPRequestHandler):
    """Implementation of the request handler.
    """

    def __init__(self, *args, **kargs):
        """Extend with new argument `state`
        """
        #TODO(chungsheng): Use argument `directory` instead after python3.7
        os.chdir(WWW_ROOT_DIR)
        super().__init__(*args, **kargs)

    def _send_json(self, data, status=200):
        """Send JSON result to client.
        """
        logging.info(f'server response:{data}')
        self.send_response(status)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def _get_argument(self, arg_name):
        arg = self._params.get(arg_name)
        if not arg:
            self._send_json({'error': f'Argument "{arg_name}"" is required.'},
                            400)
            raise StopHandleRequest()
        return arg

    def _scan(self):
        """Scan device and do some action which depend on device status

        The device should be in three state, 'locked', 'opened', 'extracted'.
        If device already has hwid and serial number, it is 'extracted', do nothing.
        If it is 'locked', ask user to solve the rma challenge.
        If it is 'opened', try to extract info from it. After extracting, lock the
        device if `is_lock_after_extracting` is true.
        """
        self._send_json(device.scan())

    def _lock(self):
        cr50_serial_name = self._get_argument('cr50SerialName')
        self._send_json({'success': device.lock(cr50_serial_name)})

    def _unlock(self):
        cr50_serial_name = self._get_argument('cr50SerialName')
        authcode = self._get_argument('authcode')
        self._send_json({'success': device.unlock(cr50_serial_name, authcode)})

    def _extract(self):
        cr50_serial_name = self._get_argument('cr50SerialName')
        board = self._get_argument('board')
        hwid, serial_number = device.extract_hwid_and_serial_number(
            cr50_serial_name, board)
        self._send_json({
            'hwid': hwid,
            'serialNumber': serial_number,
        })

    def do_POST(self):
        try:
            path = urlparse.urlparse(self.path).path

            if self.headers.get('Content-Type') != 'application/json':
                self._send_json({
                    'error': 'Only accept json as post payload.',
                }, 400)
                return
            length = int(self.headers.get('Content-Length'))
            self._params = json.loads(self.rfile.read(length))
            logging.info(f'POST {path}, params: {self._params}')
            if path == '/scan':
                return self._scan()
            if path == '/lock':
                return self._lock()
            if path == '/unlock':
                return self._unlock()
            if path == '/extract':
                return self._extract()

            self._send_json({
                'error': 'Not found',
            }, 404)
        except StopHandleRequest:
            pass
        except Exception as e:
            self._send_json(
                {
                    'error': repr(e),
                    'traceback': traceback.format_exc()
                }, 500)
            raise


def parse_arguments(raw_args):
    """Parse command line arguments"""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-p',
        '--port',
        type=int,
        default=8000,
        help='Port to run the http server.')
    parser.add_argument(
        '-v',
        '--verbosity',
        action='count',
        default=0,
        help='Logging verbosity.')
    args = parser.parse_args(raw_args)
    return args


def main(raw_args):
    """main function"""
    args = parse_arguments(raw_args)
    logging.basicConfig(level=logging.WARNING - args.verbosity * 10)
    server_address = ('', args.port)
    httpd = server.HTTPServer(server_address, RequestHandler)
    logging.info(
        f'Starting HWID Extractor server on http://localhost:{args.port}')
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    logging.info('HWID Extractor server stoped.')


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
