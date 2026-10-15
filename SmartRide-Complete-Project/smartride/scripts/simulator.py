"""Hardware-free SmartRide demo. Run the Flask app first, then this script."""
import json
import math
import time
import urllib.request
from datetime import datetime, timedelta, timezone

BASE = "http://127.0.0.1:5000/api/device"
KEY = "smartride-demo-device-key"


def call(path, data=None):
    body = json.dumps(data).encode() if data is not None else None
    req = urllib.request.Request(BASE + path, data=body, headers={"Content-Type": "application/json", "X-Device-Key": KEY})
    with urllib.request.urlopen(req, timeout=3) as response:
        return json.loads(response.read())


def helmet(lat, lon, speed, accident="normal", deadline=None, alcohol=False):
    return call("/helmet", {"helmet_worn": True, "alcohol_detected": alcohol, "battery": 86,
                            "latitude": lat, "longitude": lon, "speed": speed,
                            "accident_status": accident, "accident_deadline": deadline, "sms_status": ""})


print("SmartRide simulator: open the dashboard, then watch a 45-second sample ride.")
lat0, lon0 = 19.0760, 72.8777
helmet(lat0, lon0, 0)
print("Helmet ready:", call("/bike"))
print("Starting ride:", call("/bike", {"bike_running": True}))
for i in range(30):
    lat = lat0 + i * 0.00012
    lon = lon0 + math.sin(i / 5) * 0.00035
    speed = 22 + math.sin(i / 4) * 8
    helmet(lat, lon, speed)
    time.sleep(1)
deadline = (datetime.now(timezone.utc) + timedelta(seconds=10)).isoformat()
for i in range(5):
    helmet(lat, lon, 0, "pending", deadline)
    time.sleep(1)
helmet(lat, lon, 0, "cancelled")
time.sleep(2)
helmet(lat, lon, 0, "normal")
print("Ending ride:", call("/bike", {"bike_running": False}))
print("Demo finished. Refresh Ride History.")

