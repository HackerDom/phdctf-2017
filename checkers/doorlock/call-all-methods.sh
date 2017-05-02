#!/bin/bash
coap -T post -p 'x' 'coap://127.0.0.1/register_lock?model=MMOODDEELL&floor=6&room=616'
coap -T post -p 'x' 'coap://127.0.0.1/add_card?lock=BBB&card=5432&tag=TTAAGG'
coap -T get 'coap://127.0.0.1/get_card?lock=AAA&card=1234'
