#!/usr/bin/env python3

import sys
import logging
import asyncio
import string
import random

from aiocoap import *

OK, CORRUPT, MUMBLE, DOWN, CHECKER_ERROR = 101, 102, 103, 104, 110

logging.basicConfig(level=logging.INFO)

def random_str(size, chars=string.ascii_uppercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))

async def main(host):
    protocol = await Context.create_client_context()

    uri = 'coap://%s/echo' % host
    payload_str = 'Hello, World! ' + random_str(32)
    payload = payload_str.encode('ascii')

    print('Sending request: ' + uri)
    request = Message(code=PUT, uri=uri, payload=payload)

    try:
        response = await protocol.request(request).response
    except Exception as e:
        print('Failed to fetch resource:')
        print(e)
    else:
        print('Result: %s\n%r' % (response.code, response.payload))
        if response.payload == payload:
            print('OK')
            exit(OK)
        else:
            print('CORRUPT. Expected: %r, but was: %r' % (payload, response.payload))
            exit(CORRUPT)

if len(sys.argv) != 2:
    print('Usage: %s HOST' % sys.argv[0])
    sys.exit(CHECKER_ERROR)

host = sys.argv[1]
asyncio.get_event_loop().run_until_complete(main(host))

