"""Package the locally built LCTech FS firmware and offline UI; never flashes."""
import argparse
import hashlib
import io
import json
import re
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
    parser.add_argument('--version', default='v' + json.loads((ROOT / 'package.json').read_text(encoding='utf-8'))['version'])
    parser.add_argument('--local-preview', action='store_true', help='Bundle the exact working-tree source snapshot without committing or publishing.')
    args = parser.parse_args()
    if not args.version or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-' for c in args.version):
        parser.error('version must contain only letters, digits, dots or hyphens')
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT).strip())
    if dirty and not args.local_preview:
        raise SystemExit('Commit the reviewed sources before packaging a release.')
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    common = {'LICENSE': (ROOT / 'LICENSE').read_bytes(), 'NOTICE': (ROOT / 'NOTICE').read_bytes()}
    common['PicoSwitch2-APACHE-2.0.txt'] = (ROOT / 'lib/PicoSwitch2-APACHE-2.0.txt').read_bytes()
    source_note = (f'Source: https://github.com/infinitytom/ds5dongle-switch-pro/tree/{commit}\n'
                   'SDK source: https://github.com/sqlCRT/bouffalo_sdk/tree/b3532953322194233f80436701e7fb6bb4ad8275\n'
                   'Experimental prerelease. Current Xbox audio / Pro2 hardware compatibility has NOT been verified.\n')
    if args.local_preview:
        source_note += 'LOCAL PREVIEW: URL above is the base commit. Exact working-tree sources are in source-snapshot.zip.\n'
        source = io.BytesIO()
        names = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=ROOT).decode('utf-8').split('\0')
        with zipfile.ZipFile(source, 'w', compression=zipfile.ZIP_DEFLATED) as target:
            for name in sorted(set(filter(None, names))):
                path = ROOT / name
                if path.is_file():
                    target.writestr(name, path.read_bytes())
        common['source-snapshot.zip'] = source.getvalue()
    common['SOURCE.txt'] = source_note.encode('utf-8')
    common['使用说明.md'] = (ROOT / 'docs/SWITCH_PRO_CN.md').read_bytes()
    common['VALIDATION.md'] = (ROOT / 'docs/VALIDATION.md').read_bytes()
    common['COMPATIBILITY_CN.md'] = (ROOT / 'docs/COMPATIBILITY_CN.md').read_bytes()
    common['NS2_CONSOLE_RESEARCH_CN.md'] = (ROOT / 'docs/NS2_CONSOLE_RESEARCH_CN.md').read_bytes()
    common['alipay-donate.jpg'] = (ROOT / 'web/assets/alipay-donate.jpg').read_bytes()
    revision = re.search(r'Pro-dev1 (r\d+)', (ROOT / 'src/usb_gamepad.c').read_text(encoding='utf-8')).group(1)
    manifest = {'version': args.version, 'firmware_revision': revision, 'source_commit': commit, 'hardware_verified': False,
                'local_preview': args.local_preview, 'uncommitted_source_snapshot': dirty}
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
                      'Open the URL printed by the launcher and connect the USB receiver.\n'
                      'Ports 8765-8775 are tried automatically; occupied ports are skipped.\n'
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
    suffix = '-local' if args.local_preview else ''
    firmware_path = out / f'ds5dongle-switch-pro-lctech616-fs-{args.version}{suffix}.zip'
    ui_path = out / f'ds5dongle-local-config-{args.version}{suffix}.zip'
    archive(firmware_path, firmware, {**manifest, 'board': 'lctech616', 'usb_speed': 'fs'})
    archive(ui_path, ui, manifest)
    outputs = [firmware_path, ui_path]
    if args.local_preview:
        combined = dict(ui)
        for name, data in firmware.items():
            if name.endswith(('.bin', '.toml', '.ini')) or name == 'SDK_LICENSE':
                combined['firmware/lctech616/' + name] = data
        combined['README.txt'] = ('LCTech BL616 / USB Full-Speed local test bundle. No automatic flashing.\n'
                                'Firmware: firmware/lctech616; UI: start-config.cmd.\n'
                                'Read COMPATIBILITY_CN.md, VALIDATION.md and the Chinese guide first.\n'
                                'Xbox audio and Pro2 require r5 firmware. Pro2 has no headset audio or complete HD Rumble 2.\n'
                                'Motion/HD diagnostics and Ryujinx USB timeout fix require r7 or newer firmware.\n'
                                'Console-oriented USB identity/pairing changes require r8; this adapter is NOT console verified.\n'
                                'Console authentication and native console play are NOT verified.\n'
                                'Exact working-tree sources: source-snapshot.zip. No remote release was published.\n').encode('utf-8')
        preview_path = out / f'ds5dongle-{args.version}-{revision}-local-test.zip'
        archive(preview_path, combined, {**manifest, 'board': 'lctech616', 'usb_speed': 'fs'})
        outputs.append(preview_path)
    checksums = ''.join(f'{sha256(p.read_bytes())}  {p.name}\n' for p in outputs)
    checksum_name = f'SHA256SUMS-{args.version}-local.txt' if args.local_preview else 'SHA256SUMS.txt'
    (out / checksum_name).write_text(checksums, encoding='ascii')
    print(checksums, end='')


if __name__ == '__main__':
    main()
