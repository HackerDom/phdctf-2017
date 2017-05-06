#!/usr/bin/env python3

import asyncio
import collections
import json
import logging

import functools
from bs4 import BeautifulSoup
import string
import time

import checklib.http
import checklib.random


class DictedModel:
    def as_dict(self, keys=None):
        result = dict(zip(self._fields, list(self)))
        if keys is not None:
            return {k: v for k, v in result.items() if k in keys}
        return result


class User(collections.namedtuple('User', ['username', 'password']), DictedModel):
    @staticmethod
    def generate_random():
        username = checklib.random.string(string.ascii_lowercase, range(10, 20),
                                          first_uppercase=True)
        password = checklib.random.string(string.ascii_letters + string.digits,
                                          range(10, 20))

        return User(username, password)


class FoodType(collections.namedtuple('FoodType', ['name', 'description', 'unit']), DictedModel):
    @staticmethod
    def generate_random():
        food_type = checklib.random.from_collection('food_type',
                                                    json_encoded=True)
        unit = checklib.random.integer(range(1, 6))

        return FoodType(food_type['name'], food_type['description'], unit)


class Refrigerator(collections.namedtuple('Refrigerator', ['id', 'title', 'description']), DictedModel):
    @staticmethod
    def generate_random(**kwargs):
        title = checklib.random.string(
            string.ascii_lowercase,
            range(10, 20),
            first_uppercase=True
        )
        description = kwargs.get('description', checklib.random.string(
            string.ascii_lowercase + ' ' * 5,
            range(100, 200),
            first_uppercase=True
        ))

        return Refrigerator(None, title, description)

    def set_id(self, id):
        return Refrigerator(id, self.title, self.description)


class Recipe(collections.namedtuple('Recipe', ['id', 'title', 'description']), DictedModel):
    @staticmethod
    def generate_random(**kwargs):
        title = checklib.random.string(
            string.ascii_lowercase,
            range(10, 20),
            first_uppercase=True
        )
        description = kwargs.get('description', checklib.random.string(
            string.ascii_lowercase + ' ' * 10,
            range(100, 200),
            first_uppercase=True
        ))

        return Recipe(None, title, description)

    def set_id(self, id):
        return Recipe(id, self.title, self.description)


class FridgeApi:
    port = 9595

    def __init__(self, host):
        self.host = host

    async def _read_line(self, reader):
        buffer = b''
        char = b''
        while char != b'\n':
            buffer += char
            char = await reader.read(1)
            if len(char) == 0:
                break
        return buffer.decode()

    async def _tcp_request(self, message, loop):
        logging.info('[API] Create connection to %s:%d', self.host, self.port)
        reader, writer = await asyncio.open_connection(self.host, self.port, loop=loop)

        logging.info('[API] Send: %r', message)
        writer.write(message.encode())
        await writer.drain()

        count_json = await self._read_line(reader)
        logging.info('[API] Received count info: %r...', count_json)
        count = json.loads(count_json)['count']

        answer = []
        for _ in range(count):
            message = await self._read_line(reader)
            logging.info('[API] Received: %r...', message)
            answer.append(message)

        logging.info('[API] Close the socket')
        writer.close()

        return '\n'.join(answer)

    def query(self, command, *params):
        message = '%s %s\n' % (command, ' '.join(map(str, params)))
        loop = asyncio.get_event_loop()
        answer = loop.run_until_complete(self._tcp_request(message, loop))
        return answer


def add_csrf_token(function, cookies):
    """ Function for adding django-specific csrfmiddlewaretoken field in each POST request"""
    @functools.wraps(function)
    def new_function(*args, **kwargs):
        data = kwargs.get('data', {})
        data['csrfmiddlewaretoken'] = cookies.get('csrftoken')
        kwargs['data'] = data
        return function(*args, **kwargs)

    return new_function


class FridgeChecker(checklib.http.HttpChecker):
    port = 8000

    def __init__(self):
        super().__init__()
        self._session.post = add_csrf_token(self._session.post, self._session.cookies)

    def info(self):
        print('vulns: 2:8')
        self.exit(checklib.StatusCode.OK)

    def check_main_page_content(self, r, logged=False):
        self.check_page_content(r, [
            'Sign in and register unlimited refrigerators, add information about your food and favourite recipes',
            'Andrew Gein aka andgein, Hackerdom team, special for PHDays 2017 Online CTF',
            '/static/js/tether.min.js',
            ])
        if logged:
            self.check_page_content(r, ['Register your own refrigerator',
                                        'Add food type',
                                        'Save them in our cloud storage and make cooking easier.',
                                        ])

    def check_main_page(self):
        logging.info('Checking main page')
        self.check_main_page_content(self.try_http_get(self.main_url))

    def try_signup(self):
        user = User.generate_random()

        logging.info('Try to signup user: %s', user)
        data = user.as_dict()
        data['password1'] = data['password2'] = data['password']
        r = self.try_http_post(self.main_url + '/accounts/register/', data=data)

        if r.url.endswith('/accounts/register/'):
            logging.error('Can\'t signup a new user: %s', user)
            self.exit(checklib.StatusCode.CORRUPT, 'Can\'t signup a new user')

        self.check_main_page_content(r, logged=True)
        return user

    def try_signin(self, user):
        logging.info('Try to signin as user: %s', user)
        r = self.try_http_post(self.main_url + '/accounts/login/',
                               data=user.as_dict())
        if r.url.endswith('/accounts/login/'):
            logging.error('Can\'t signin as user: %s', user)
            self.exit(checklib.StatusCode.CORRUPT,
                      'Can\'t signin as a registered user')
        self.check_main_page_content(r, logged=True)

    def try_create_food_type(self):
        food_type = FoodType.generate_random()
        logging.info('Try to add new food type: %s', food_type)

        r = self.try_http_post(self.main_url + '/foods/add/',
                               data=food_type.as_dict())
        if r.url.endswith('/foods/add/'):
            logging.error('Can\'t add food type: %s', food_type)
            self.exit(checklib.StatusCode.CORRUPT,
                      'Can\'t add food type for the registered user')
        return food_type

    def try_create_refrigerator(self, description=None):
        if description is None:
            refrigerator = Refrigerator.generate_random()
        else:
            refrigerator = Refrigerator.generate_random(description=description)

        logging.info('Try to add new refrigerator: %s', refrigerator)

        r = self.try_http_post(
            self.main_url + '/refrigerators/add/',
            data=refrigerator.as_dict(['title', 'description'])
        )
        if r.url.endswith('/refrigerators/add/'):
            logging.error('Can\'t add refrigerator: %s', refrigerator)
            self.exit(checklib.StatusCode.CORRUPT,
                      'Can\'t add refrigerator for the registered user')

        logging.info('Extract refrigerator id from url %s', r.url)
        return refrigerator.set_id(int(r.url.split('/')[-2]))

    def try_create_recipe(self, description):
        if description is None:
            recipe = Recipe.generate_random()
        else:
            recipe = Recipe.generate_random(description=description)

        logging.info('Try to add new recipe: %s', recipe)

        r = self.try_http_post(self.main_url + '/recipes/add/',
                               data=recipe.as_dict(['title', 'description']))
        if r.url.endswith('/recipes/add/'):
            logging.error('Can\'t add recipe: %s', recipe)
            self.exit(checklib.StatusCode.CORRUPT,
                      'Can\'t add recipe for the registered user')

        return recipe.set_id(int(r.url.split('/')[-2]))

    def try_add_food_to_refrigerator(self, refrigerator, food_type, food_count):
        refrigerator_url = self.main_url + '/refrigerators/%d/' % (refrigerator.id, )
        logging.info('Loading refrigerator page: %s', refrigerator_url)

        r = self.try_http_get(refrigerator_url)
        soup = BeautifulSoup(r.text, 'html.parser')
        select = soup.find_all('select', {'name': 'food_type'})[0]
        food_type_id = None
        for option in select.find_all('option'):
            if option.text == food_type.name:
                food_type_id = option['value']

        if food_type_id is None:
            logging.error('Can\'t find food type %s on the page %s', food_type, refrigerator_url)
            self.exit(checklib.StatusCode.CORRUPT, 'Can\'t find food type on the page %s' % (refrigerator_url, ))
        logging.info('Found food type id: %s', food_type_id)

        logging.info('Try to add food of this type to the refrigerator %s', refrigerator)
        r = self.try_http_post(
            self.main_url + '/refrigerators/%d/add/' % (refrigerator.id,),
            data={
                'food_type': food_type_id,
                'count': food_count,
            }
        )
        logging.info('Okay, lets check that food has been appeared in the refrigerator')
        self.check_page_content(r, [food_type.name + ' ('], 'Can\'t find food in the refrigerator')

    # TODO: copy-paste with try_add_food_to_refrigerator()?
    def try_add_item_to_recipe(self, recipe, food_type, food_count):
        recipe_url = self.main_url + '/recipes/%d/' % (recipe.id,)
        logging.info('Loading recipe page: %s', recipe_url)

        r = self.try_http_get(recipe_url)
        soup = BeautifulSoup(r.text, 'html.parser')
        select = soup.find_all('select', {'name': 'food_type'})[0]
        food_type_id = None
        for option in select.find_all('option'):
            if option.text == food_type.name:
                food_type_id = option['value']

        if food_type_id is None:
            logging.error('Can\'t find food type %s on the page %s', food_type, recipe_url)
            self.exit(checklib.StatusCode.CORRUPT,
                      'Can\'t find food type on the page %s' % (recipe_url,))
        logging.info('Found food type id: %s', food_type_id)

        logging.info('Try to add food of this type to the recipe %s', recipe)
        r = self.try_http_post(
            self.main_url + '/recipes/%d/add/' % (recipe.id,),
            data={
                'food_type': food_type_id,
                'count': food_count,
                'what_to_do': checklib.random.string(string.ascii_lowercase + ' ' * 5, range(50, 100), first_uppercase=True),
                'pause_after': checklib.random.integer(range(1, 20)),
            }
        )
        logging.info('Okay, lets check that food has been appeared in the recipe')
        self.check_page_content(r, [food_type.name + ' ('], 'Can\'t find food in the recipe')

    @checklib.http.build_main_url
    def check(self, address):
        self.check_main_page()
        # TODO: check api service on port 9595

    @checklib.http.build_main_url
    def put(self, address, flag_id, flag, vuln):
        # Get CSRF token
        self.try_http_get(self.main_url)

        user = self.try_signup()
        food_type = self.try_create_food_type()
        refrigerator = self.try_create_refrigerator(
            description=flag if vuln == 1 else None)
        recipe = self.try_create_recipe(description=flag if vuln == 2 else None)

        refrigerator_count = checklib.random.integer(range(10, 20))
        recipe_count = checklib.random.integer(range(5, refrigerator_count))
        self.try_add_food_to_refrigerator(refrigerator, food_type,
                                          refrigerator_count)
        self.try_add_item_to_recipe(recipe, food_type, recipe_count)

        logging.info('Now I\'ll print flag_id to STDOUT. '
                     'My flag_id is serialized information about user, '
                     'refrigerator and recipe')
        flag_id = json.dumps({
            'user': user.as_dict(),
            'refrigerator': refrigerator.as_dict(),
            'recipe': recipe.as_dict(),
        })
        print(flag_id)

    @checklib.http.build_main_url
    def get(self, address, flag_id, flag, vuln):
        try:
            deserialized = json.loads(flag_id)
            user = User(**deserialized['user'])
            refrigerator = Refrigerator(**deserialized['refrigerator'])
            recipe = Recipe(**deserialized['recipe'])
        except Exception as e:
            logging.error('Can\'t extract information from flag_id:')
            logging.exception(e)
            self.exit(checklib.StatusCode.ERROR)
            return

        logging.info('Extracted user from flag_id: %s', user)
        logging.info('Extracted refrigerator from flag_id: %s', refrigerator)
        logging.info('Extracted recipe from flag_id: %s', recipe)

        # Get CSRF token
        self.try_http_get(self.main_url)

        if vuln == 1:
            self.try_signin(user)
            r = self.try_http_get(self.main_url)
            found = flag in r.text
        elif vuln == 2:
            logging.info('Sleep 5 seconds before sending request to the web server')
            time.sleep(5)
            logging.info('Send any request to the web server')
            self.try_http_get(self.main_url)
            logging.info('Sleep 1 second after sending request to the web server and before API request')
            time.sleep(1)

            api = FridgeApi(address)
            api.query('LIST', user.username, user.password)
            api_answer = api.query('RECIPES', user.username, user.password, refrigerator.id)

            found = flag in api_answer
        else:
            self.exit(checklib.StatusCode.ERROR, 'Invalid vulnerability id: %d' % (vuln, ))
            return

        if found:
            logging.info('I found flag: %s', flag)
        else:
            logging.info('I can\'t found flag')
            where = ''
            if vuln == 1:
                where = 'in the refrigerator info'
            elif vuln == 2:
                where = 'in the recipe info'
            self.exit(checklib.StatusCode.CORRUPT, 'Can\'t find the flag %s' % (where, ))


if __name__ == '__main__':
    FridgeChecker().run()
