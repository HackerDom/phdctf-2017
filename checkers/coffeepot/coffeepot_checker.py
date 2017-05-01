#!/usr/bin/env python3

import requests
import sys
import random

from user_agents import USER_AGENTS

OK, CORRUPT, MUMBLE, DOWN, CHECKER_ERROR = 101, 102, 103, 104, 110


def http_req(*args, **kwargs):
    req_kwparams = {"timeout": 5,
                    # "proxies": {"http": "http://127.0.0.1:8888"},
                    "headers": {}}
    req_kwparams.update(kwargs)
    req_kwparams["headers"]["User-Agent"] = random.choice(USER_AGENTS)
    req_kwparams["headers"]["Cache-Control"] = "max-age=0"
    req_kwparams["headers"]["Upgrade-Insecure-Requests"] = "1"
    req_kwparams["headers"]["Accept"] = ("text/html,application/xhtml+xml,"
                                         "application/xml;q=0.9,image/webp,"
                                         "*/*;q=0.8")
    req_kwparams["headers"]["Accept-Encoding"] = "gzip, deflate, sdch, br"
    req_kwparams["headers"]["Accept-Language"] = "ru,en-US;q=0.8,en;q=0.6"

    return requests.request(*args, **req_kwparams)


def verdict(exit_code, public="", private=""):
    if public:
        print(public)
    if private:
        print(private, file=sys.stderr)
    sys.exit(exit_code)


def gen_pot():
    return random.randrange(10**31, 10**32)


def gen_amount():
    return random.randrange(0, 10**6)


def check(ip):
    resp = http_req("GET", "http://%s:3255/" % ip)
    if resp.status_code != 200:
        verdict(MUMBLE, "GET / is not 200", "GET / is %d" % resp.status_code)
    if "coffee" not in resp.text:
        verdict(MUMBLE, "No coffee in /?")

    pot = gen_pot()
    milk, almond, whisky, rum, flag = [gen_amount() for i in range(5)]

    additions = "Whole-milk;%s,Almond;%s,Whisky;%s,Rum;%s,Flag;%s" % (
        milk, almond, whisky, rum, flag)
    headers = {"Accept-Additions": additions}

    resp = http_req("BREW", "http://%s:3255/pot-%s" % (ip, pot),
                    headers=headers)
    if resp.status_code != 200 or "BREWING" not in resp.text:
        verdict(MUMBLE, "Doesn't brew")

    addition = random.choice(["Whole-milk", "Almond", "Whisky", "Rum"])
    headers = {"Addition": addition}
    resp = http_req("PUT", "http://%s:3255/pot-%s" % (ip, pot),
                    headers=headers)
    if resp.status_code != 200 or "ADDED" not in resp.text:
        verdict(MUMBLE, "Failed to add an ingridient", "PUT stage")

    resp = http_req("PROPFIND", "http://%s:3255/pot-%s" % (ip, pot))
    if resp.status_code != 200:
        verdict(MUMBLE, "Failed to get coffee metadata")

    for num in (milk+1, almond+1, whisky+1, rum+1):
        if str(num) in resp.text:
            break
    else:
        verdict(MUMBLE, "Failed to add an ingridient", "PROPFIND stage")

    verdict(OK)


def put(ip, flag_id, flag):
    pot = gen_pot()
    flag_id = pot

    milk, almond, whisky, rum = [gen_amount() for i in range(4)]

    additions = "Whole-milk;%s,Almond;%s,Whisky;%s,Rum;%s,Flag;%s" % (
        milk, almond, whisky, rum, flag)
    headers = {"Accept-Additions": additions}

    resp = http_req("BREW", "http://%s:3255/pot-%s" % (ip, pot),
                    headers=headers)
    if resp.status_code != 200 or "BREWING" not in resp.text:
        verdict(MUMBLE, "Doesn't brew")

    verdict(OK, pot)


def get(ip, flag_id, flag):
    resp = http_req("PROPFIND", "http://%s:3255/pot-%s" % (ip, flag_id))
    if resp.status_code != 200:
        verdict(MUMBLE, "Failed to get coffee metadata")
    if flag not in resp.text:
        verdict(MUMBLE, "No such flag")

    verdict(OK)


def main(action, args):
    try:
        if action == "info":
            verdict(OK, "vulns: 1")
        elif action == "check":
            check(ip=args[0])
        elif action == "put":
            put(ip=args[0], flag_id=args[1], flag=args[2])
        elif action == "get":
            get(ip=args[0], flag_id=args[1], flag=args[2])
        else:
            verdict(CHECKER_ERROR, "Checker error", "Wrong action")
    except requests.ConnectionError as e:
        verdict(DOWN, "", e)
    except requests.RequestException as e:
        verdict(MUMBLE, "", e)


if __name__ == "__main__":
    try:
        main(sys.argv[1], sys.argv[2:])
        verdict(CHECKER_ERROR, "Checker error", "No verdict")
    except Exception as e:
        verdict(CHECKER_ERROR, "Checker error", "Checker error: %s" % e)
