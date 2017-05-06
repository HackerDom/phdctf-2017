import os.path
import random
import checklib.utils
import json


def string(chars, count, first_uppercase=False):
    if type(count) is not int:
        count = random.choice(count)

    if count < 0:
        raise Exception('Can\'t generate string with %d chars' % count)

    result = ''.join([random.choice(chars) for _ in range(count)])

    if first_uppercase and len(result) > 0:
        result = result[0].upper() + result[1:]

    return result


def integer(variants):
    return random.choice(variants)


def from_collection(name, json_encoded=False):
    try_files = [
        os.path.join(checklib.utils.checker_location(), 'collections.data',
                     name),
        os.path.join(checklib.utils.checklib_location(), 'collections.data',
                     name),
    ]
    for filename in try_files:
        if os.path.exists(filename):
            with open(filename, 'r', encoding='utf-8') as f:
                collection = f.readlines()
            break
    else:
        raise ValueError('Can\'t find collection %s' % (name,))

    collection = [s.rstrip() for s in collection]
    if json_encoded:
        collection = [json.loads(s) for s in collection]
    return random.choice(collection)


def firstname():
    return from_collection('firstname')


def lastname():
    return from_collection('lastname')


def useragent():
    return from_collection('useragent')
