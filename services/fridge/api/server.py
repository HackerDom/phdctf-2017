#!/usr/bin/env python3

import asyncio
import loader
import os.path
from django.contrib.auth import hashers
import json
import logging


PORT = 9595
DUMPS_DIRECTORY = os.path.join(os.path.dirname(__file__), '..', 'dumps')


class Models:
    def __init__(self):
        self.loader = loader.ModelLoader()
        try:
            self.update()
        except:
            pass

    def update(self):
        self.users = self.loader.load_models_from_file(
            os.path.join(DUMPS_DIRECTORY, 'auth_user.csv'))
        self.refrigerators = self.loader.load_models_from_file(
            os.path.join(DUMPS_DIRECTORY, 'refrigerator_refrigerator.csv'))
        self.food_items = self.loader.load_models_from_file(
            os.path.join(DUMPS_DIRECTORY, 'refrigerator_fooditem.csv')
        )
        self.food_types = self.loader.load_models_from_file(
            os.path.join(DUMPS_DIRECTORY, 'refrigerator_foodtype.csv')
        )
        self.recipes = self.loader.load_models_from_file(
            os.path.join(DUMPS_DIRECTORY, 'refrigerator_recipe.csv')
        )
        self.recipe_items = self.loader.load_models_from_file(
            os.path.join(DUMPS_DIRECTORY, 'refrigerator_recipeitem.csv')
        )

    @staticmethod
    def filter_by_field(models, **kwargs):
        if len(kwargs) != 1:
            raise ValueError()

        field_name, field_value = list(kwargs.items())[0]

        return [
            m for m in models.values() if getattr(m, field_name) == field_value
        ]


models = Models()
password_hasher = hashers.PBKDF2PasswordHasher()


async def read_line(reader):
    buffer = b''
    char = b''
    while char != b'\n':
        buffer += char
        char = await reader.read(1)
        if len(char) == 0:
            break
    return buffer.decode()


def check_password(password, django_hash):
    return password_hasher.verify(password, django_hash)


def authorize(username, password):
    for user in models.users.values():
        if user.username == username:
            if check_password(password, user.password):
                return user
            else:
                return None
    return None


async def process_list_command(writer, user):
    owned_refrigerators = [
        r for r in models.refrigerators.values() if r.owner_id == user.id
    ]

    writer.write((json.dumps({
        'count': len(owned_refrigerators)
    }) + '\n').encode())

    for refrigerator in owned_refrigerators:
        writer.write((json.dumps({
            'id': refrigerator.id,
            'title': refrigerator.title,
            'description': refrigerator.description,
        }) + '\n').encode())

    await writer.drain()


def check_recipe_for_foods_in_refrigerator(recipe, food_items):
    recipe_items = models.filter_by_field(
        models.recipe_items, recipe_id=recipe.id
    )
    if len(recipe_items) == 0:
        return None

    is_fit = False
    my_food_needs_to_buy = []
    food_items = {i.food_type_id: i.count for i in food_items}
    for item in recipe_items:
        food_type_id = item.food_type_id
        count = item.count
        if food_type_id in food_items and count <= food_items[food_type_id]:
            is_fit = True
        else:
            my_food_needs_to_buy.append(item)

    if is_fit:
        return my_food_needs_to_buy
    return None


async def process_recipes_command(writer, user, refrigerator_id):
    refrigerator = models.refrigerators[refrigerator_id]
    if refrigerator.owner_id != user.id:
        writer.write(b'Invalid refrigerator id\n')
        await writer.drain()
        return

    food_items = models.filter_by_field(
        models.food_items, refrigerator_id=refrigerator.id
    )

    answer = []

    for recipe in models.recipes.values():
        food_needs_to_buy = check_recipe_for_foods_in_refrigerator(recipe, food_items)
        if food_needs_to_buy is not None:
            answer.append({
                'id': recipe.id,
                'title': recipe.title,
                'description': recipe.description,
                'need_to_buy': [
                    {
                        'food_type': models.food_types[f.food_type_id].name,
                        'count': f.count
                    } for f in food_needs_to_buy]
            })

    writer.write((json.dumps({
        'count': len(answer)
    }) + '\n').encode())

    for obj in answer:
        writer.write((json.dumps(obj) + '\n').encode())

    await writer.drain()

async def process_command(reader, writer):
    command_line = await read_line(reader)
    if command_line == '':
        return True

    logging.info(
        'Received "%s" from %s',
        command_line,
        writer.get_extra_info('peername')
    )

    command = command_line.split()
    if command[0] == 'EXIT':
        return True

    if len(command) < 3:
        raise ValueError()

    username = command[1]
    password = command[2]
    user = authorize(username, password)
    if user is None:
        writer.write(b'Invalid username or password\n')
        await writer.drain()
        return False

    if command[0] == 'LIST':
        if len(command) != 3:
            ValueError('Invalid number of arguments in command LIST')
        await process_list_command(writer, user)
    if command[0] == 'RECIPES':
        if len(command) != 4:
            ValueError('Invalid number of arguments in command RECIPES')
        await process_recipes_command(writer, user, int(command[3]))

    return False

async def handle_client(reader, writer):
    try:
        while True:
            try:
                if await process_command(reader, writer):
                    break
            except Exception as e:
                writer.write('Please follow the protocol. {}\n'.format(e).encode())
                await writer.drain()
    finally:
        writer.close()


async def update_models_loop():
    while True:
        try:
            models.update()
        except Exception as e:
            print('Exception while updating models: {}'.format(e))
        await asyncio.sleep(1)


if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)-15s [%(levelname)s] %(message)s', level=logging.DEBUG)

    loop = asyncio.get_event_loop()
    server_coroutine = asyncio.start_server(handle_client, '0.0.0.0', PORT, loop=loop)
    update_models_coroutine = update_models_loop()
    loop.create_task(update_models_coroutine)
    server = loop.run_until_complete(server_coroutine)

    # Serve requests until Ctrl+C is pressed
    logging.info('Serving on {}'.format(server.sockets[0].getsockname()))
    try:
        loop.run_forever()
    except KeyboardInterrupt:
        pass

    # Close the server
    server.close()
    loop.run_until_complete(server.wait_closed())
    loop.close()
