import checklib
import checklib.utils
import checklib.random
import logging
import requests


def build_main_url(fn):
    def wrapper(self, address, *args, **kwargs):
        self.main_url = '%s://%s:%d' % (self.proto, address, self.port)
        return fn(self, address, *args, **kwargs)
    wrapper.__name__ = fn.__name__
    wrapper.__doc__ = fn.__doc__
    return wrapper


# noinspection PyPep8Naming
class default_param:
    def __init__(self, name, value):
        self.name = name
        self.value = value

    def __call__(self, fn):
        def wrapper(*args, **kwargs):
            if isinstance(self.value, dict):
                kwargs[self.name] = checklib.utils.merge_dicts(self.value, kwargs.get(self.name, {}))
                logging.info('Auto-configure default dict-like param "%s": %s', self.name, kwargs[self.name])
            else:
                kwargs[self.name] = kwargs.get(self.name, self.value)
                logging.info('Auto-configure default param "%s": %s', self.name, kwargs[self.name])
            return fn(*args, **kwargs)
        wrapper.__name__ = fn.__name__
        wrapper.__doc__ = fn.__doc__
        return wrapper


class HttpChecker(checklib.Checker):
    port = 80
    proto = 'http'

    def __init__(self):
        super().__init__()
        logging.info('Created requests session')
        self._session = requests.Session()

    def _check_response(self, response):
        if response.status_code >= 400:
            self.exit(
                checklib.StatusCode.DOWN,
                'Got HTTP status code %d on %s' % (response.status_code, response.url)
            )
        if response.status_code != 200:
            self.exit(
                checklib.StatusCode.MUMBLE,
                'Got HTTP status code %d on %s' % (response.status_code, response.url)
            )
        return response

    @default_param('headers', {'User-Agent': checklib.random.useragent()})
    def try_http_get(self, url, *args, **kwargs):
        return self._check_response(self._session.get(url, *args, **kwargs))

    @default_param('headers', {'User-Agent': checklib.random.useragent()})
    def try_http_post(self, url, *args, **kwargs):
        return self._check_response(self._session.post(url, *args, **kwargs))

    def check_page_content(self, response, strings_for_check, failed_message=None):
        message = 'Invalid page content at %url'
        if failed_message is not None:
            message += ': ' + failed_message
        if '%url' in message:
            message = message.replace('%url', response.url)

        for s in strings_for_check:
            self.mumble_if_false(
                s in response.text,
                message,
                'Can\'t find string "%s" in response from %s' % (s, response.url)
            )

        logging.info(
            'Checked %d string(s) on page %s, all of them exist',
            len(strings_for_check),
            response.url
        )
