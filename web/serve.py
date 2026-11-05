"""Offline static server. Binds only to this computer; no packages needed."""
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from functools import partial
from pathlib import Path
import argparse
import errno
import socket
import urllib.request
import webbrowser

class LocalServer(ThreadingHTTPServer):
    # HTTPServer's SO_REUSEADDR can allow multiple live listeners on Windows.
    allow_reuse_address = False

    def server_bind(self):
        if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--no-browser', action='store_true')
    args = parser.parse_args()
    url = 'http://127.0.0.1:8765'
    try:
        server = LocalServer(('127.0.0.1', 8765), partial(SimpleHTTPRequestHandler, directory=str(Path(__file__).parent)))
    except OSError as error:
        if error.errno not in (errno.EADDRINUSE, 10048) and getattr(error, 'winerror', None) != 10048:
            raise
        try:
            with urllib.request.urlopen(url + '/config.js', timeout=2) as response:
                existing = response.read(256).startswith(b'// Byte offsets come from the packed config_body')
        except (OSError, ValueError):
            existing = False
        if not existing:
            raise SystemExit('Port 8765 is in use by another application. Close it and try again.')
        print('DS5 local config is already running: ' + url, flush=True)
        if not args.no_browser:
            webbrowser.open(url)
        raise SystemExit(0)
    print('DS5 local config: ' + url + ' (Ctrl+C to stop)', flush=True)
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
