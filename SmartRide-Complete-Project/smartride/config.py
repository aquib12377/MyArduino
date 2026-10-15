import os
import sys
from pathlib import Path


BASE_DIR = Path(__file__).resolve().parent
if getattr(sys, "frozen", False):
    default_data = Path(os.getenv("LOCALAPPDATA", Path.home())) / "SmartRide" / "data"
else:
    default_data = BASE_DIR / "data"
DATA_DIR = Path(os.getenv("SMARTRIDE_DATA_DIR", default_data))
DATA_DIR.mkdir(parents=True, exist_ok=True)

SECRET_KEY = os.getenv("SMARTRIDE_SECRET_KEY", "smartride-local-prototype-change-me")
DEVICE_API_KEY = os.getenv("SMARTRIDE_DEVICE_API_KEY", "smartride-demo-device-key")
DATABASE_PATH = Path(os.getenv("SMARTRIDE_DATABASE", DATA_DIR / "smartride.db"))
HOST = os.getenv("SMARTRIDE_HOST", "0.0.0.0")
PORT = int(os.getenv("SMARTRIDE_PORT", "5000"))
HELMET_TIMEOUT_SECONDS = int(os.getenv("SMARTRIDE_HELMET_TIMEOUT_SECONDS", "10"))
