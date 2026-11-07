"""Exercise the real socket behavior that differs on Windows and POSIX."""
import importlib.util
from http.server import SimpleHTTPRequestHandler
from pathlib import Path
import unittest
import threading
import tempfile
from functools import partial

spec = importlib.util.spec_from_file_location('ds5_local_server', Path(__file__).resolve().parents[1] / 'web/serve.py')
server_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server_module)


class ServerTests(unittest.TestCase):
    def test_reuse_requires_matching_assets_and_skips_old_tools(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'config.js').write_bytes(b'// Byte offsets come from the packed config_body\n')
            for name in ('dashboard.js', 'dashboard.css', 'index.html'):
                (root / name).write_bytes(b'current version')
            server = server_module.LocalServer(('127.0.0.1', 0), partial(SimpleHTTPRequestHandler, directory=folder))
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            port = server.server_port
            try:
                url = f'http://127.0.0.1:{port}'
                self.assertEqual(server_module.existing_config(url, root), 'same')
                reused, target = server_module.select_server(root, [port])
                self.assertIsNone(reused)
                self.assertEqual(target, url)
                with tempfile.TemporaryDirectory() as new_folder:
                    newer = Path(new_folder)
                    for item in root.iterdir():
                        (newer / item.name).write_bytes(item.read_bytes())
                    (newer / 'dashboard.js').write_bytes(b'new Xbox UI')
                    self.assertEqual(server_module.existing_config(url, newer), 'outdated')
                    fresh, target = server_module.select_server(newer, [port, 0])
                    self.assertIsNotNone(fresh)
                    fresh.server_close()
                (root / 'config.js').write_bytes(b'other application')
                with self.assertRaises(SystemExit):
                    server_module.select_server(root, [port])
            finally:
                server.shutdown()
                server.server_close()
                thread.join()

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
