from django.template import Library

register = Library()


@register.filter
def get_item(obj, key):
    return obj.__getitem__(key)
