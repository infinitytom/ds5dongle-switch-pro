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

def existing_config(url, root):
    """Reuse only identical UI assets; older tools may still own port 8765."""
    try:
        with urllib.request.urlopen(url + '/config.js', timeout=2) as response:
            config = response.read(262144)
        if not config.startswith(b'// Byte offsets come from the packed config_body'):
            return 'foreign'
    except (OSError, ValueError):
        return 'foreign'
    for name in ('config.js', 'dashboard.js', 'dashboard.css', 'index.html'):
        try:
            with urllib.request.urlopen(url + '/' + name, timeout=2) as response:
                if response.read(262144) != (root / name).read_bytes():
                    return 'outdated'
        except (OSError, ValueError):
            return 'outdated'
    return 'same'


def select_server(root, ports=range(8765, 8776)):
    for port in ports:
        url = f'http://127.0.0.1:{port}'
        try:
            server = LocalServer(('127.0.0.1', port), partial(SimpleHTTPRequestHandler, directory=str(root)))
            return server, f'http://127.0.0.1:{server.server_port}'
        except OSError as error:
            if error.errno not in (errno.EADDRINUSE, 10048) and getattr(error, 'winerror', None) != 10048:
                raise
        existing = existing_config(url, root)
        if existing == 'same':
            return None, url
        # Any occupied port may belong to another application or an older
        # launcher. Leave that service running and try the next local port.
    raise SystemExit('No available DS5 configuration port. Ports 8765-8775 are occupied; close an unused local service and try again.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--no-browser', action='store_true')
    args = parser.parse_args()
    server, url = select_server(Path(__file__).parent)
    if server is None:
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
