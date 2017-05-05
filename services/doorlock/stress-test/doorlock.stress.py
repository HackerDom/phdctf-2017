#!/usr/bin/env python3

import sys
import logging
import asyncio
import string
import random
import datetime
from doorlockclient import DoorlockClient

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)-15s|%(levelname)-5s|%(message)s',
    filename='doorlock.stress.log'
)

COUNT_OK, COUNT_ERR = 0, 0

def random_str(size, chars):
    return ''.join(random.choice(chars) for _ in range(size))

async def make_task(host):
    global COUNT_OK, COUNT_ERR
    client = DoorlockClient(host)

    model = random_str(4, string.ascii_uppercase)
    floor = random_str(1, "123456789")
    room = floor + random_str(3, string.digits)
    code, lock = await client.register_lock(model, floor, room)

    card = random_str(1, "123456789")
    tag  = random_str(32, string.ascii_uppercase)
    await client.add_card(lock, card, tag)

    code, tag_check = await client.get_card(lock, card)
    if tag == tag_check:
        COUNT_OK += 1
    else:
        COUNT_ERR += 1
        logging.getLogger().error("Expected '%s', but got '%s'", tag, tag_check)

if len(sys.argv) != 3:
    print('Usage: %s HOST COUNT' % sys.argv[0])
    sys.exit(-1)

host, count = sys.argv[1], int(sys.argv[2])

loop = asyncio.get_event_loop()

t_start = datetime.datetime.now()
tasks = [make_task(host) for i in range(count)]

loop.run_until_complete(asyncio.wait(tasks))

req_per_count = 3
sec = (datetime.datetime.now() - t_start).microseconds/1e6
log_str = '%d RPS (%d req in %f sec)' % (req_per_count*count/sec, req_per_count*count, sec)
logging.getLogger().info(log_str)
logging.getLogger().info('OK: %d, ERR: %d', COUNT_OK, COUNT_ERR)

loop.close()
