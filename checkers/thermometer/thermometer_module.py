#!/usr/bin/env python3

import sys
import requests
from requests.auth import HTTPBasicAuth

OK, CORRUPT, MUMBLE, DOWN, CHECKER_ERROR = 101, 102, 103, 104, 110

def trace(public="", private=""):
    if public:
        print(public)
    if private:
        print(private, file=sys.stderr)

class ThermometerModule:
    def __init__(self, host):
        self._base_url = 'http://%s:8888/' % host

    def check_index(self):
        try:
            r = requests.get(self._base_url, auth=HTTPBasicAuth('admin', 'webrelay'), timeout=3)
            if r.status_code != 200:
                trace("Bad HTTP status code", "Bad HTTP status code: %d" % r.status_code)
                return MUMBLE
            if "Authorize temperature sensor" in r.text:
                return OK
            trace("Bad response", "Response html does not contain 'Authorize temperature sensor'")
            return MUMBLE
        except requests.exceptions.ConnectionError as e:
            trace("Connection error", "Connection error at ThermometerModule.index(): %s" % e)
            return DOWN
        except requests.exceptions.Timeout as e:
            trace("Timeout", "Timeout at ThermometerModule.index(): %s" % e)
            return DOWN
        except Exception as e:
            trace("Unknown error", "Unknown request error: %s" % e)
            return CHECKER_ERROR

    def authorize_sensor(self, sensor_id, auth_token):
        try:
            r = requests.post(self._base_url + 'sensors', data = {'sensor': sensor_id, 'token' : auth_token}, auth=HTTPBasicAuth('admin', 'webrelay'), timeout=3)
            if r.status_code != 200:
                trace("Bad HTTP status code", "Bad HTTP status code: %d" % r.status_code)
                return MUMBLE
            return OK
        except requests.exceptions.ConnectionError as e:
            trace("Connection error", "Connection error at ThermometerModule.index(): %s" % e)
            return DOWN
        except requests.exceptions.Timeout as e:
            trace("Timeout", "Timeout at ThermometerModule.index(): %s" % e)
            return DOWN
        except Exception as e:
            trace("Unknown error", "Unknown request error: %s" % e)
            return CHECKER_ERROR
