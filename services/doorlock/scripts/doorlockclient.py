#!/usr/bin/env python3

import logging
import asyncio
import datetime
import aiocoap

class DoorlockClient:
    def __init__(self, host):
        self._log = logging.getLogger()
        self._base_url = 'coap://%s/' % host

    def _build_uri(self, method, args):
        uri = self._base_url + method
        if args:
            uri += '?'
            separator = ''
            for key, val in args.items():
                uri += separator + key + '=' + str(val)
                separator = '&'
        return uri

    async def _send(self, code, method, args):
        protocol = await aiocoap.Context.create_client_context()
        uri = self._build_uri(method, args)
        request = aiocoap.Message(code=code, uri=uri)
        self._log.info('--> %s', uri)
        try:
            t_start = datetime.datetime.now()
            response = await protocol.request(request).response
        except Exception as e:
            diff = datetime.datetime.now() - t_start;
            t_diff = diff.seconds*1000 + diff.microseconds/1000
            self._log.error('Exception: %s in %d ms', e, t_diff)
            return None, None
        else:
            diff = datetime.datetime.now() - t_start;
            t_diff = diff.seconds*1000 + diff.microseconds/1000
            self._log.info('<-- [%s] %r in %d ms', response.code, response.payload, t_diff)
            return response.code, response.payload.decode('ascii')

    async def register_lock(self, model, floor, room):
        return await self._send(aiocoap.POST, 'register_lock', {'model': model, 'floor': floor, 'room': room})

    async def add_card(self, lock, card, tag):
        return await self._send(aiocoap.POST, 'add_card', {'lock': lock, 'card': card, 'tag': tag})

    async def get_card(self, lock, card):
        return await self._send(aiocoap.GET, 'get_card', {'lock': lock, 'card': card})

    async def index(self):
        return await self._send(aiocoap.GET, '', {})
