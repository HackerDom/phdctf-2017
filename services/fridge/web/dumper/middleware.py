import os.path
import shutil
import random
import time

from django.core.cache import cache
from django.utils import timezone
from django.conf import settings
from django.contrib.auth.models import User

from . import dump_model_to_file
from refrigerator import models


DUMPED_MODELS = [models.Refrigerator,
                 models.FoodType,
                 models.FoodItem,
                 models.Recipe,
                 models.RecipeItem,
                 User,
                 ]


class DumperMiddleware:
    def __init__(self, get_response):
        self.get_response = get_response

    def __call__(self, request):
        response = self.get_response(request)

        last_dump_time = cache.get('last_dump_time')
        # Run dump for models only
        # if last dump has been passed 5 seconds ago or more
        if (last_dump_time is None or
           (timezone.now() - last_dump_time).total_seconds() >= 5):
            cache.set('last_dump_time', timezone.now())

            for model in DUMPED_MODELS:
                filename = os.path.join(settings.FRIDGE_DUMP_DIRECTORY,
                                        model._meta.db_table + '.csv')
                tmp_filename = (filename + '.' +
                                str(random.randint(1, 9999)).rjust(4, '0'))
                try:
                    dump_model_to_file(model, tmp_filename)

                    # Try 5 times with pause 0.1 seconds
                    for i in range(5):
                        try:
                            shutil.move(tmp_filename, filename)
                            break
                        except:
                            time.sleep(0.1)
                except:
                    # Don't throw exception to the user, just wait next request
                    cache.set('last_dump_time', last_dump_time)

        return response
