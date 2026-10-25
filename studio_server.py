#!/usr/bin/env python3
"""Local-only launcher for Magneticraft2 Studio Linux alpha.

Uses only Python 3's standard library; no cloud, no installer, no daemon.
Running the browser as a separate app window is intentional until we settle
on a native Linux desktop host. This launcher is NOT a Minecraft mod runtime.
"""
import argparse
import functools
import http.server
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        if self.path not in ('/favicon.ico',):
            print('[studio] ' + fmt % args, flush=True)

    def end_headers(self):
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()


def main():
    parser = argparse.ArgumentParser(description='Run the local Magneticraft2 Studio Linux editor')
    parser.add_argument('--no-browser', action='store_true', help='Print local URL without opening a window')
    parser.add_argument('--port', type=int, default=0, help='Choose a TCP port (default: automatic free local port)')
    options = parser.parse_args()
    handler = functools.partial(QuietHandler, directory=str(ROOT))
    with http.server.ThreadingHTTPServer(('127.0.0.1', options.port), handler) as server:
        url = f'http://127.0.0.1:{server.server_address[1]}/'
        print(f'Magneticraft2 Studio 0.1\n  Local editor: {url}\n  Press Ctrl+C to stop.\n', flush=True)
        browser = None
        if not options.no_browser:
            browser = next((shutil.which(cmd) for cmd in (
                'chromium', 'chromium-browser', 'google-chrome-stable',
                'google-chrome', 'brave-browser', 'brave', 'vivaldi',
                'microsoft-edge-stable', 'microsoft-edge') if shutil.which(cmd)), None)
            if browser:
                profile = Path(os.getenv('XDG_CACHE_HOME', str(Path.home()/'.cache'))) / 'magneticraft2-studio' / 'chromium-profile'
                profile.mkdir(parents=True, exist_ok=True)
                args = [browser, f'--app={url}', f'--user-data-dir={profile}', '--no-first-run', '--disable-features=TranslateUI', '--class=Magneticraft2Studio']
                try:
                    proc = subprocess.Popen(args)
                    try:
                        server.timeout = 0.4
                        while proc.poll() is None:
                            server.handle_request()
                    except KeyboardInterrupt:
                        pass
                    print('Studio window closed. Server stopped.')
                    return
                except OSError as exc:
                    print(f'Failed to launch Chromium app window: {exc}', file=sys.stderr)
            opener = shutil.which('xdg-open')
            if opener:
                subprocess.Popen([opener, url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print('If no browser opens, copy the local URL printed above.')
        try:
            server.serve_forever(poll_interval=.2)
        except KeyboardInterrupt:
            print('\nStudio stopped.')


if __name__ == '__main__':
    main()
