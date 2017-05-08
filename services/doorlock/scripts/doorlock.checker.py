#!/usr/bin/env python3

import os
import sys
import asyncio
import string
import random
import datetime
import traceback
from doorlockclient import DoorlockClient

COUNTERS_DIR = "doorlock.counters"
OK, CORRUPT, MUMBLE, DOWN, CHECKER_ERROR = 101, 102, 103, 104, 110

def trace(message):
    print(message, file=sys.stderr)

def verdict(exit_code, public="", private=""):
    if public:
        print(public)
    if private:
        print(private, file=sys.stderr)
    sys.exit(exit_code)

def random_str(size, chars):
    return ''.join(random.choice(chars) for _ in range(size))

def do_async(task, timeout=3):
    time_limited_task = asyncio.wait_for(task, timeout)
    return asyncio.get_event_loop().run_until_complete(time_limited_task)

def info():
    verdict(OK, "vulns: 1")

def check(args):
    if len(args) != 1:
        verdict(CHECKER_ERROR, "Not enough args", "Not enough args for check()")
    host = args[0]
    trace("check(%s)" % host)

    client = DoorlockClient(host)
    try:
        code, data = do_async(client.index())
    except asyncio.TimeoutError:
        verdict(DOWN, "Timeout", "Timeout at client.index()")
    if not code:
        verdict(DOWN, "Connection error", "Connection error at /")
    elif code.dotted != "2.05":
        verdict(MUMBLE, "Bad status code", "Bad status code at /: %s" % code)
    elif data != "DOORLOCK Service 5.19 built May 2017":
        verdict(MUMBLE, "Bad response", "Bad response at /: '%s'" % data)
    else:
        verdict(OK)

def put(args):
    if len(args) != 3:
        verdict(CHECKER_ERROR, "Not enough args", "Not enough args for put()")
    host, flag_id, flag_data = args
    trace("put(%s, %s, %s)" % (host, flag_id, flag_data))

    model = random_str(4, string.ascii_uppercase)
    floor = random_str(1, "123456789")
    room = floor + random_str(3, string.digits)

    trace("register_lock(model=%s, floor=%s, room=%s)" % (model, floor, room))

    client = DoorlockClient(host)
    try:
        code, lock_id = do_async(client.register_lock(model, floor, room))
    except asyncio.TimeoutError:
        verdict(DOWN, "Timeout", "Timeout at register_lock")
    if not code:
        verdict(DOWN, "Connection error", "Connection error at register_lock")
    elif code.dotted != "2.05":
        verdict(MUMBLE, "Bad status code", "Bad status code at register_lock: %s" % code)
    elif len(lock_id) != 9:
        verdict(MUMBLE, "Bad response data", "Bad lock_id length (must be 9): %s" % lock_id)

    trace("registered, lock_id: %s" % lock_id)

    counter = 0
    counter_file_name = COUNTERS_DIR + "/" + host
    if os.path.isfile(counter_file_name):
        with open(counter_file_name) as f:
            counter = int(next(f))
            trace("loaded counter (card_id) from file: %d" % counter)

    counter += 1
    with open(counter_file_name, 'w') as f:
        f.write(str(counter))
    trace("saved new counter (card_id) to file: %s" % counter)

    trace("add_card(%s, %s, %s)" % (lock_id, counter, flag_data))

    try:
        code, data = do_async(client.add_card(lock_id, counter, flag_data))
    except asyncio.TimeoutError:
        verdict(DOWN, "Timeout", "Timeout at add_card")
    if not code:
        verdict(DOWN, "Connection error", "Connection error at add_card")
    elif code.dotted != "2.05":
        verdict(MUMBLE, "Bad status code", "Bad status code at add_card: %s" % code)
    elif data != "OK":
        verdict(MUMBLE, "Bad response data", "Bad response data (should be 'OK'): %s" % data)

    trace("added card successfully")

    flag_id = "%s-%s" % (lock_id, counter)
    trace("new flag_id: " + flag_id)

    verdict(OK, flag_id)

def get(args):
    if len(args) != 3:
        verdict(CHECKER_ERROR, "Not enough args", "Not enough args for get()")
    host, flag_id, flag_data = args
    trace("get(%s, %s, %s)" % (host, flag_id, flag_data))

    lock_id, card_id = flag_id.split("-")

    client = DoorlockClient(host)
    try:
        code, data = do_async(client.get_card(lock_id, card_id))
    except asyncio.TimeoutError:
        verdict(DOWN, "Timeout", "Timeout at get_card")
    if not code:
        verdict(DOWN, "Connection error", "Connection error at get_card")
    elif code.dotted != "2.05":
        verdict(MUMBLE, "Bad status code", "Bad status code at get_card: %s" % code)
    elif data != flag_data:
        verdict(CORRUPT, "No or wrong flag", "Got flag value: '%s', expected: '%s'" % (data, flag_data))

    trace("OK: flag found")
    verdict(OK)

def main(args):
    if len(args) == 0:
        verdict(CHECKER_ERROR, "No args")
    try:
        if args[0] == "info":
            info()
        elif args[0] == "check":
            check(args[1:])
        elif args[0] == "put":
            put(args[1:])
        elif args[0] == "get":
            get(args[1:])
        else:
            verdict(CHECKER_ERROR, "Checker error", "Wrong action: " + args[0])
    except Exception as e:
        verdict(DOWN, "Down", "Exception: %s" % traceback.format_exc())

if __name__ == "__main__":
    os.makedirs(COUNTERS_DIR, exist_ok=True)
    try:
        main(sys.argv[1:])
        verdict(CHECKER_ERROR, "Checker error (NV)", "No verdict")
    except Exception as e:
        verdict(CHECKER_ERROR, "Checker error (CE)", "Exception: %s" % e)
