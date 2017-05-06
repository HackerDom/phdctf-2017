import csv
import codecs


def dump_model_to_file(model, filename):
    # Use Django API to get all model fields
    columns = [field.attname for field in model._meta.fields]

    with codecs.open(filename, 'w', encoding='utf-8') as opened_file:
        # Open csv writer and dump header: special row with columns names
        writer = csv.DictWriter(opened_file, fieldnames=columns)
        writer.writeheader()

        # Iterate over all objects of the model
        for obj in model.objects.all():
            object_dict = {}
            for column in columns:
                object_dict[column] = str(getattr(obj, column, ''))

            # Dump dictionary for current object
            writer.writerow(object_dict)
