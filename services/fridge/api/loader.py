import codecs
import csv


class Model:
    def __init__(self, dictionary):
        for key, value in dictionary.items():
            if value is None:
                value = '0'
            if value.isnumeric():
                value = int(value)
            setattr(self, key, value)

    def __repr__(self):
        return 'Model' + str(self.__dict__)


class ModelLoader:
    @staticmethod
    def load_models_from_file(filename):
        objects = {}
        with codecs.open(filename, 'r', encoding='utf-8') as opened_file:
            reader = csv.DictReader(opened_file)
            for row in reader:
                model = Model(row)
                objects[model.id] = model
        return objects
