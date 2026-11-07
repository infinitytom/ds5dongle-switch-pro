"""Probe Switch sensors through the emulator's SDL3; never saves dongle config.

Example: py -3 tools/probe_switch_sdl.py --sdl E:/Ryujinx-Nextendo/publish/SDL3.dll
Add --rumble for a brief, low-amplitude Pro-1 HD packet test (then explicit stop).
Close the emulator first to avoid two SDL clients negotiating the same device.
"""
import argparse
import ctypes as c
import json
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdl', required=True)
    parser.add_argument('--seconds', type=float, default=2)
    parser.add_argument('--rumble', action='store_true')
    args = parser.parse_args()
    if not 0.5 <= args.seconds <= 20:
        parser.error('--seconds must be between 0.5 and 20')
    lib = c.CDLL(args.sdl)

    def api(name, result, arguments):
        function = getattr(lib, name)
        function.restype, function.argtypes = result, arguments
        return function

    error = api('SDL_GetError', c.c_char_p, [])
    init = api('SDL_Init', c.c_bool, [c.c_uint32])
    pump = api('SDL_PumpEvents', None, [])
    quit_sdl = api('SDL_Quit', None, [])
    get_ids = api('SDL_GetGamepads', c.POINTER(c.c_uint32), [c.POINTER(c.c_int)])
    free = api('SDL_free', None, [c.c_void_p])
    open_pad = api('SDL_OpenGamepad', c.c_void_p, [c.c_uint32])
    close = api('SDL_CloseGamepad', None, [c.c_void_p])
    name = api('SDL_GetGamepadName', c.c_char_p, [c.c_void_p])
    vendor = api('SDL_GetGamepadVendor', c.c_uint16, [c.c_void_p])
    product = api('SDL_GetGamepadProduct', c.c_uint16, [c.c_void_p])
    has = api('SDL_GamepadHasSensor', c.c_bool, [c.c_void_p, c.c_int])
    enable = api('SDL_SetGamepadSensorEnabled', c.c_bool, [c.c_void_p, c.c_int, c.c_bool])
    sensor_data = api('SDL_GetGamepadSensorData', c.c_bool, [c.c_void_p, c.c_int, c.POINTER(c.c_float), c.c_int])
    effect = api('SDL_SendGamepadEffect', c.c_bool, [c.c_void_p, c.c_void_p, c.c_int])
    version = api('SDL_GetVersion', c.c_int, [])()
    report = {'sdl_version': version, 'devices': [], 'config_written': False}
    if not init(0x200 | 0x2000):
        raise SystemExit(error().decode(errors='replace'))
    try:
        for _ in range(10):
            pump()
            time.sleep(0.02)
        count = c.c_int()
        ids = get_ids(c.byref(count))
        try:
            selected = [ids[i] for i in range(count.value)]
        finally:
            if ids:
                free(ids)
        for device_id in selected:
            pad = open_pad(device_id)
            if not pad:
                report['devices'].append({'opened': False, 'error': error().decode(errors='replace')})
                continue
            try:
                if vendor(pad) != 0x057e or product(pad) not in (0x2009, 0x2069):
                    continue
                row = {'name': name(pad).decode(errors='replace'), 'vendor': vendor(pad),
                       'product': product(pad), 'sensors': {}, 'samples': []}
                for kind, label in ((1, 'accel_m_s2'), (2, 'gyro_rad_s')):
                    present = has(pad, kind)
                    row['sensors'][label] = {'present': present, 'enabled': present and enable(pad, kind, True)}
                until = time.monotonic() + args.seconds
                next_sample = 0
                while time.monotonic() < until:
                    pump()
                    if time.monotonic() >= next_sample:
                        sample = {}
                        for kind, label in ((1, 'accel_m_s2'), (2, 'gyro_rad_s')):
                            vector = (c.c_float * 3)()
                            sample[label] = list(vector) if sensor_data(pad, kind, vector, 3) else None
                        row['samples'].append(sample)
                        next_sample = time.monotonic() + 0.1
                    time.sleep(0.01)
                if args.rumble and product(pad) == 0x2009:
                    neutral = [0, 1, 0x40, 0x40]

                    def send(packet):
                        data = (c.c_uint8 * len(packet))(*packet)
                        return bool(effect(pad, data, len(packet)))

                    row['hd_packet_results'] = []
                    try:
                        for side in (0, 1):
                            packet = [0x10, 0] + neutral + neutral
                            packet[2 + side * 4:6 + side * 4] = [0, 0x21, 0x40, 0x48]
                            row['hd_packet_results'].append(send(packet))
                            for _ in range(5):
                                pump()
                                time.sleep(0.025)
                            send([0x10, 0] + neutral + neutral)
                    finally:
                        row['explicit_stop_accepted'] = send([0x10, 0] + neutral + neutral)
                report['devices'].append(row)
            finally:
                close(pad)
    finally:
        quit_sdl()
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
