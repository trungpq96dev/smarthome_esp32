"""
ESP32 gia lap - chay thu toan bo giao dien tren may tinh, khong can bo that.

Chay:      python chay-thu.py
Trang noi bo (nhu tren bo that):  http://127.0.0.1:8899/
Trang dieu khien tu xa qua 4G:    http://127.0.0.1:8899/tu-xa

Ca hai trang dung CHUNG mot con ESP32 gia lap, nen mo song song hai cua so
se thay chung dong bo voi nhau.

Logic bom, chong chay can, gioi han thoi gian va khoa lien dong van deu lam
giong firmware, ke ca cac moc thoi gian. Day la ban mo phong HANH VI de thu
giao dien, khong phai code C++ that dang chay tren bo.
"""

import json, re, pathlib, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

HERE = pathlib.Path(__file__).parent
ROOT = HERE.parent
PORT = 8899

# Cac moc thoi gian, lay dung theo config.h
MIN_REST_S    = 2 * 60
MAX_RUN_S     = 45 * 60
DRY_CHECK_S   = 45
MIN_FLOW_LPM  = 1.0
VALVE_TRAVEL_S = 15

lock = threading.Lock()

S = {
    "simMode": True,
    "simAboveLow": True, "simIsFull": False, "simFlow": 20.0,
    "tankMode": "auto", "tankOn": False, "tankStart": 0.0, "tankStop": 0.0,
    "boostOn": False,
    "valveTarget": False, "valveOpen": False, "valveMoving": False, "valveAt": 0.0,
    "dryLocked": False, "fault": "",
    "dryCheck": 45, "minRest": 2, "maxRun": 45,
    "boot": time.time(),
}


def aboveLow():  return S["simAboveLow"]
def isFull():    return S["simIsFull"]
def fTank():     return S["simFlow"] if S["tankOn"] else 0.0
def fBoost():    return S["simFlow"] if S["boostOn"] else 0.0


def set_fault(msg):
    if not S["fault"]:
        S["fault"] = msg
        print("  [LOI]", msg, flush=True)


def drive_tank(on):
    if on == S["tankOn"]:
        return
    S["tankOn"] = on
    if on: S["tankStart"] = time.time()
    else:  S["tankStop"] = time.time()
    print("  [BOM BON]", "BAT" if on else "TAT", flush=True)


def drive_boost(on):
    if on == S["boostOn"]:
        return
    S["boostOn"] = on
    print("  [BOM AP]", "BAT" if on else "TAT", flush=True)


def tick():
    now = time.time()

    if S["valveMoving"] and now - S["valveAt"] >= VALVE_TRAVEL_S:
        S["valveMoving"] = False
        S["valveOpen"] = S["valveTarget"]
        print("  [VAN]", "MO" if S["valveOpen"] else "DONG", flush=True)

    want = False
    if S["tankMode"] == "on":
        want = True
    elif S["tankMode"] == "auto":
        want = (not isFull()) if S["tankOn"] else (not aboveLow())

    if S["tankOn"]:
        run = now - S["tankStart"]
        max_run_s = S.get("maxRun", 45) * 60
        dry_check_s = S.get("dryCheck", 45)
        if run > max_run_s:
            want = False
            set_fault("Bom len bon chay qua %d phut ma chua day. Da tat de an toan."
                      % (max_run_s // 60))
        elif run > dry_check_s and fTank() < MIN_FLOW_LPM:
            want = False
            S["dryLocked"] = True
            set_fault("Bom len bon chay nhung khong co nuoc. Da tat va khoa.")
    else:
        min_rest_s = S.get("minRest", 2) * 60
        if S["tankStop"] and now - S["tankStop"] < min_rest_s:
            want = False

    if S["dryLocked"]: want = False
    if isFull():       want = False
    drive_tank(want)

    if S["boostOn"] and (S["valveOpen"] or S["valveMoving"]):
        drive_boost(False)
        set_fault("Da tat bom ap vi van bypass dang mo.")


def exec_cmd(k, v):
    if k == "tank" and v in ("auto", "on", "off"):
        S["tankMode"] = v
    elif k == "booster":
        drive_boost(v == "on")
    elif k == "valve":
        want = (v == "open")
        if want != S["valveTarget"]:
            if want: drive_boost(False)
            S["valveTarget"] = want
            S["valveMoving"] = True
            S["valveAt"] = time.time()
            print("  [VAN] dang quay sang", "MO" if want else "DONG", flush=True)
    elif k == "clear":
        S["fault"] = ""; S["dryLocked"] = False; S["tankStop"] = 0.0
        print("  [LOI] da xoa", flush=True)
    elif k == "sim":         S["simMode"] = (v == "1")
    elif k == "simAboveLow": S["simAboveLow"] = (v == "1")
    elif k == "simIsFull":   S["simIsFull"] = (v == "1")
    elif k == "simFlow":
        try: S["simFlow"] = float(v)
        except ValueError: pass
    elif k == "cfgDry":
        try:
            val = float(v)
            if 5 <= val <= 600: S["dryCheck"] = int(val)
        except ValueError: pass
    elif k == "cfgRest":
        try:
            val = float(v)
            if 0.1 <= val <= 180: S["minRest"] = val if val % 1 != 0 else int(val)
        except ValueError: pass
    elif k == "cfgRun":
        try:
            val = float(v)
            if 1 <= val <= 360: S["maxRun"] = int(val)
        except ValueError: pass


def state_json():
    now = time.time()
    io = [
        ("Relay 1 · bơm áp",      18, S["boostOn"]),
        ("Relay 2 · bơm lên bồn", 19, S["tankOn"]),
        ("Relay 3 · van mở",      23, S["valveTarget"]),
        ("Relay 4 · van đóng",    13, not S["valveTarget"]),
        ("Vào · phao mức thấp",   25, aboveLow()),
        ("Vào · phao mức đầy",    26, isFull()),
    ]
    return {
        "sim": 1 if S["simMode"] else 0,
        "up": int(now - S["boot"]),
        "rssi": -58,
        "aboveLow": 1 if aboveLow() else 0,
        "isFull": 1 if isFull() else 0,
        "lvl": -1, "litres": -1, "temp": 28.5,
        "tankOn": 1 if S["tankOn"] else 0,
        "tankMode": S["tankMode"],
        "tankRun": int(now - S["tankStart"]) if S["tankOn"] else 0,
        "fTank": round(fTank(), 1),
        "boostOn": 1 if S["boostOn"] else 0,
        "fBoost": round(fBoost(), 1),
        "valve": "moving" if S["valveMoving"] else ("open" if S["valveOpen"] else "closed"),
        "locked": 1 if S["dryLocked"] else 0,
        "fault": S["fault"],
        "fw": "1.5 (gia lap)",
        "dryCheck": S.get("dryCheck", 45),
        "minRest": S.get("minRest", 2),
        "maxRun": S.get("maxRun", 45),
        "temps": [
            {"n": "Nhiệt độ bồn mái", "v": 28.5},
            {"n": "Nhiệt độ bình nóng 1", "v": 61.3},
        ],
        "ssid": "WiFi-gia-lap", "ip": "127.0.0.1",
        "io": [{"n": n, "g": g, "v": 1 if v else 0} for n, g, v in io],
    }


SCAN = [
    {"s": "WiFi-gia-lap", "r": -52},
    {"s": "Nha-hang-xom", "r": -71},
    {"s": "Viettel-A3F2", "r": -84},
]


def local_page():
    src = (ROOT / "page.h").read_text(encoding="utf-8")
    return re.search(r'R"PAGE\((.*)\)PAGE";', src, re.S).group(1)


def remote_page():
    html, n = re.subn(
        r'<script src="https://unpkg\.com/mqtt[^"]*"></script>',
        '<script src="/shim.js"></script>',
        (ROOT / "dieu-khien.html").read_text(encoding="utf-8"),
    )
    assert n == 1, "khong tim thay the script mqtt trong dieu-khien.html"
    return html


class H(BaseHTTPRequestHandler):
    def _send(self, body, ctype, code=200):
        b = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def _json(self, obj, code=200):
        self._send(json.dumps(obj, ensure_ascii=False), "application/json; charset=utf-8", code)

    def do_GET(self):
        u = urlparse(self.path)
        p = u.path
        try:
            if p in ("/", "/index.html"):
                self._send(local_page(), "text/html; charset=utf-8")
            elif p in ("/tu-xa", "/tu-xa/"):
                self._send(remote_page(), "text/html; charset=utf-8")
            elif p == "/shim.js":
                self._send((HERE / "shim.js").read_text(encoding="utf-8"),
                           "application/javascript; charset=utf-8")
            elif p == "/api/state":
                with lock:
                    self._json(state_json())
            elif p == "/api/cmd":
                with lock:
                    for k, vs in parse_qs(u.query, keep_blank_values=True).items():
                        exec_cmd(k, vs[0])
                    tick()
                    self._json(state_json())
            elif p == "/api/scan":
                time.sleep(1.2)          # quet that cung mat vai giay
                self._json(SCAN)
            elif p == "/api/wifi":
                self._json({"ok": 0, "msg": "Trinh gia lap khong doi duoc WiFi that"}, 400)
            else:
                self.send_error(404)
        except Exception as e:
            self._json({"ok": 0, "msg": str(e)}, 500)

    def do_POST(self):
        if urlparse(self.path).path == "/api/ota":
            self._json({"ok": 0, "msg": "Trinh gia lap khong nap duoc firmware"}, 400)
        else:
            self.send_error(404)

    def log_message(self, *a):
        pass


def ticker():
    while True:
        time.sleep(1)
        with lock:
            tick()


if __name__ == "__main__":
    threading.Thread(target=ticker, daemon=True).start()
    print()
    print("  ESP32 gia lap dang chay")
    print("  ----------------------------------------------")
    print("  Trang noi bo (nhu tren bo that) : http://127.0.0.1:%d/" % PORT)
    print("  Trang dieu khien tu xa          : http://127.0.0.1:%d/tu-xa" % PORT)
    print("  ----------------------------------------------")
    print("  Nhan Ctrl+C de dung")
    print()
    try:
        ThreadingHTTPServer(("127.0.0.1", PORT), H).serve_forever()
    except KeyboardInterrupt:
        print("\n  Da dung.\n")
