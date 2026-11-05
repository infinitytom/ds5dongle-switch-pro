"""Exercise the real socket behavior that differs on Windows and POSIX."""
import importlib.util
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('ds5_local_server', Path(__file__).resolve().parents[1] / 'web/serve.py')
server_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server_module)


class ServerTests(unittest.TestCase):
    def test_live_listener_cannot_be_bound_twice(self):
        server = server_module.LocalServer(('127.0.0.1', 0), SimpleHTTPRequestHandler)
        try:
            with self.assertRaises(OSError):
                duplicate = server_module.LocalServer(server.server_address, SimpleHTTPRequestHandler)
                duplicate.server_close()
        finally:
            server.server_close()


if __name__ == '__main__':
    unittest.main()
