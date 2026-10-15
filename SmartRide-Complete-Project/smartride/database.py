import sqlite3
from contextlib import contextmanager
from datetime import datetime, timezone
from werkzeug.security import generate_password_hash

import config


def utc_now():
    return datetime.now(timezone.utc).isoformat()


@contextmanager
def connect():
    db = sqlite3.connect(config.DATABASE_PATH, timeout=10)
    db.row_factory = sqlite3.Row
    db.execute("PRAGMA foreign_keys = ON")
    try:
        yield db
        db.commit()
    finally:
        db.close()


SCHEMA = """
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL CHECK(role IN ('rider','guardian')),
    phone TEXT DEFAULT '',
    emergency_phone TEXT DEFAULT '',
    rider_id INTEGER,
    created_at TEXT NOT NULL,
    FOREIGN KEY(rider_id) REFERENCES users(id)
);
CREATE TABLE IF NOT EXISTS device_state (
    id INTEGER PRIMARY KEY CHECK(id=1),
    helmet_worn INTEGER NOT NULL DEFAULT 0,
    alcohol_detected INTEGER NOT NULL DEFAULT 0,
    latitude REAL,
    longitude REAL,
    speed REAL NOT NULL DEFAULT 0,
    battery REAL NOT NULL DEFAULT 0,
    accident_status TEXT NOT NULL DEFAULT 'normal',
    accident_deadline TEXT,
    bike_running INTEGER NOT NULL DEFAULT 0,
    ignition_allowed INTEGER NOT NULL DEFAULT 0,
    helmet_updated_at TEXT,
    bike_updated_at TEXT,
    last_sms_status TEXT DEFAULT ''
);
CREATE TABLE IF NOT EXISTS rides (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    rider_id INTEGER NOT NULL,
    started_at TEXT NOT NULL,
    ended_at TEXT,
    distance_km REAL NOT NULL DEFAULT 0,
    max_speed REAL NOT NULL DEFAULT 0,
    average_speed REAL NOT NULL DEFAULT 0,
    duration_seconds INTEGER NOT NULL DEFAULT 0,
    harsh_accel_count INTEGER NOT NULL DEFAULT 0,
    harsh_brake_count INTEGER NOT NULL DEFAULT 0,
    helmet_off_events INTEGER NOT NULL DEFAULT 0,
    alcohol_events INTEGER NOT NULL DEFAULT 0,
    accident_events INTEGER NOT NULL DEFAULT 0,
    safety_score INTEGER,
    score_explanation TEXT DEFAULT '',
    FOREIGN KEY(rider_id) REFERENCES users(id)
);
CREATE TABLE IF NOT EXISTS ride_points (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ride_id INTEGER NOT NULL,
    recorded_at TEXT NOT NULL,
    latitude REAL,
    longitude REAL,
    speed REAL NOT NULL DEFAULT 0,
    FOREIGN KEY(ride_id) REFERENCES rides(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS safety_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ride_id INTEGER,
    event_type TEXT NOT NULL,
    severity TEXT NOT NULL DEFAULT 'info',
    message TEXT NOT NULL,
    latitude REAL,
    longitude REAL,
    created_at TEXT NOT NULL,
    FOREIGN KEY(ride_id) REFERENCES rides(id) ON DELETE SET NULL
);
INSERT OR IGNORE INTO device_state(id) VALUES (1);
"""


def init_db():
    with connect() as db:
        db.executescript(SCHEMA)
        columns = {row["name"] for row in db.execute("PRAGMA table_info(rides)").fetchall()}
        if "duration_seconds" not in columns:
            db.execute("ALTER TABLE rides ADD COLUMN duration_seconds INTEGER NOT NULL DEFAULT 0")
        rider = db.execute("SELECT id FROM users WHERE email=?", ("rider@smartride.local",)).fetchone()
        if not rider:
            db.execute(
                "INSERT INTO users(name,email,password_hash,role,phone,emergency_phone,created_at) VALUES(?,?,?,?,?,?,?)",
                ("Demo Rider", "rider@smartride.local", generate_password_hash("Rider@123"), "rider", "", "+919999999999", utc_now()),
            )
            rider_id = db.execute("SELECT last_insert_rowid()").fetchone()[0]
        else:
            rider_id = rider["id"]
        if not db.execute("SELECT id FROM users WHERE email=?", ("guardian@smartride.local",)).fetchone():
            db.execute(
                "INSERT INTO users(name,email,password_hash,role,phone,emergency_phone,rider_id,created_at) VALUES(?,?,?,?,?,?,?,?)",
                ("Demo Guardian", "guardian@smartride.local", generate_password_hash("Guardian@123"), "guardian", "+919999999999", "", rider_id, utc_now()),
            )


def row_dict(row):
    return dict(row) if row else None
