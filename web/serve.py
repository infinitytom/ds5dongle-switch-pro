"""Offline static server. Binds only to this computer; no packages needed."""
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from functools import partial
from pathlib import Path
import webbrowser

if __name__ == '__main__':
    server = ThreadingHTTPServer(('127.0.0.1', 8765), partial(SimpleHTTPRequestHandler, directory=str(Path(__file__).parent)))
    print('DS5 local config: http://localhost:8765 (Ctrl+C to stop)', flush=True)
    webbrowser.open('http://localhost:8765')
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
