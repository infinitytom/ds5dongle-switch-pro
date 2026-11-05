"""Package the locally built LCTech FS firmware and offline UI; never flashes."""
import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def archive(path, files, manifest):
    with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as target:
        for name, data in files.items():
            target.writestr(name, data)
        target.writestr('manifest.json', json.dumps({**manifest, 'files': {
            name: {'bytes': len(data), 'sha256': sha256(data)} for name, data in files.items()
        }}, indent=2, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', default='v0.1.0-alpha.3')
    args = parser.parse_args()
    if not args.version or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-' for c in args.version):
        parser.error('version must contain only letters, digits, dots or hyphens')
    if subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT).strip():
        raise SystemExit('Commit the reviewed sources before packaging a release.')
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    common = {'LICENSE': (ROOT / 'LICENSE').read_bytes(), 'NOTICE': (ROOT / 'NOTICE').read_bytes()}
    source_note = (f'Source: https://github.com/infinitytom/ds5dongle-switch-pro/tree/{commit}\n'
                   'SDK source: https://github.com/sqlCRT/bouffalo_sdk/tree/b3532953322194233f80436701e7fb6bb4ad8275\n'
                   'Experimental prerelease. Hardware compatibility has NOT been verified.\n')
    common['SOURCE.txt'] = source_note.encode('utf-8')
    common['使用说明.md'] = (ROOT / 'docs/SWITCH_PRO_CN.md').read_bytes()
    common['VALIDATION.md'] = (ROOT / 'docs/VALIDATION.md').read_bytes()
    common['alipay-donate.jpg'] = (ROOT / 'web/assets/alipay-donate.jpg').read_bytes()
    manifest = {'version': args.version, 'source_commit': commit, 'hardware_verified': False}
    firmware = dict(common)
    firmware['README.txt'] = ('LCTech BL616 / USB Full-Speed only. Experimental, NOT hardware verified.\n'
                            'Use the included boot2, partition and firmware with the upstream flashing guide.\n'
                            'Do not flash other board models. Keep a backup of your existing firmware.\n'
                            'Check SHA256SUMS.txt and read the included Chinese guide before flashing.\n'
                            'Optional support: alipay-donate.jpg (infinitytom).\n').encode('utf-8')
    for name in ('ds5dongle-lctech616.bin', 'boot2_bl616_isp_release_v8.1.8.bin',
                 'partition.bin', 'partition_cfg_4M_nosec.toml', 'flash_prog_cfg.ini'):
        firmware[name] = (ROOT / 'firmware/lctech616' / name).read_bytes()
    sdk_license = ROOT.parent / 'bouffalo_sdk/LICENSE'
    if not sdk_license.is_file():
        raise SystemExit('SDK LICENSE is required when distributing its boot2 image.')
    firmware['SDK_LICENSE'] = sdk_license.read_bytes()
    firmware['SHA256SUMS.txt'] = ''.join(f'{sha256(data)}  {name}\n' for name, data in firmware.items()
                                       if name.endswith('.bin')).encode('ascii')
    ui = dict(common)
    ui['README.txt'] = ('Offline configuration: install Python 3 and desktop Chrome or Edge.\n'
                      'Windows: double-click start-config.cmd.\n'
                      'Linux/macOS: python3 web/serve.py\n'
                      'Open http://localhost:8765 and connect the USB receiver.\n'
                      'No internet or npm install is required to use the configuration page.\n'
                      'Switch Pro mode requires this project firmware; hardware compatibility is unverified.\n'
                      'Optional support: alipay-donate.jpg (infinitytom).\n').encode('utf-8')
    for name in ('start-config.cmd', 'start-config.ps1'):
        ui[name] = (ROOT / name).read_bytes()
    for path in sorted((ROOT / 'web').rglob('*')):
        if path.is_file() and '__pycache__' not in path.parts:
            ui[path.relative_to(ROOT).as_posix()] = path.read_bytes()
    out = ROOT / 'release'
    out.mkdir(exist_ok=True)
    firmware_path = out / f'ds5dongle-switch-pro-lctech616-fs-{args.version}.zip'
    ui_path = out / f'ds5dongle-local-config-{args.version}.zip'
    archive(firmware_path, firmware, {**manifest, 'board': 'lctech616', 'usb_speed': 'fs'})
    archive(ui_path, ui, manifest)
    checksums = ''.join(f'{sha256(p.read_bytes())}  {p.name}\n' for p in (firmware_path, ui_path))
    (out / 'SHA256SUMS.txt').write_text(checksums, encoding='ascii')
    print(checksums, end='')


if __name__ == '__main__':
    main()
