import math
import os
import socket
import sys
import threading
import time
import webbrowser
from datetime import datetime, timezone
from functools import wraps
from pathlib import Path

from flask import Flask, abort, flash, jsonify, redirect, render_template, request, session, url_for
from flask_socketio import SocketIO
from werkzeug.security import check_password_hash, generate_password_hash

import config
from database import connect, init_db, row_dict, utc_now
from scoring import calculate_safety_score


def resource_path(relative):
    root = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return root / relative


app = Flask(__name__, template_folder=resource_path("templates"), static_folder=resource_path("static"))
app.secret_key = config.SECRET_KEY
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")
runtime = {"active_ride_id": None, "last_speed": 0.0, "last_point": None, "last_point_time": None}


def parse_time(value):
    if not value:
        return None
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def logged_in(view):
    @wraps(view)
    def wrapped(*args, **kwargs):
        if "user_id" not in session:
            return redirect(url_for("login"))
        return view(*args, **kwargs)
    return wrapped


def role_required(role):
    def decorator(view):
        @wraps(view)
        def wrapped(*args, **kwargs):
            if session.get("role") != role:
                abort(403)
            return view(*args, **kwargs)
        return wrapped
    return decorator


def device_authorized():
    supplied = request.headers.get("X-Device-Key") or request.args.get("key")
    return supplied == config.DEVICE_API_KEY


def require_device(view):
    @wraps(view)
    def wrapped(*args, **kwargs):
        if not device_authorized():
            return jsonify(error="invalid device key"), 401
        return view(*args, **kwargs)
    return wrapped


def helmet_is_fresh(state):
    updated = parse_time(state.get("helmet_updated_at"))
    if not updated:
        return False
    return (datetime.now(timezone.utc) - updated).total_seconds() <= config.HELMET_TIMEOUT_SECONDS


def public_state(state):
    state = dict(state)
    fresh = helmet_is_fresh(state)
    allowed = (fresh and bool(state["helmet_worn"]) and not bool(state["alcohol_detected"])
               and state["accident_status"] != "pending")
    state["helmet_online"] = fresh
    state["ignition_allowed"] = allowed
    state["helmet_worn"] = bool(state["helmet_worn"])
    state["alcohol_detected"] = bool(state["alcohol_detected"])
    state["bike_running"] = bool(state["bike_running"])
    return state


def haversine_km(lat1, lon1, lat2, lon2):
    r = 6371.0
    a1, a2 = math.radians(lat1), math.radians(lat2)
    da, dl = math.radians(lat2-lat1), math.radians(lon2-lon1)
    a = math.sin(da/2)**2 + math.cos(a1)*math.cos(a2)*math.sin(dl/2)**2
    return r * 2 * math.atan2(math.sqrt(a), math.sqrt(1-a))


def current_ride_id(db):
    if runtime["active_ride_id"]:
        return runtime["active_ride_id"]
    row = db.execute("SELECT id FROM rides WHERE ended_at IS NULL ORDER BY id DESC LIMIT 1").fetchone()
    runtime["active_ride_id"] = row["id"] if row else None
    return runtime["active_ride_id"]


def start_ride(db):
    rider = db.execute("SELECT id FROM users WHERE role='rider' ORDER BY id LIMIT 1").fetchone()
    if not rider:
        return None
    db.execute("INSERT INTO rides(rider_id,started_at) VALUES(?,?)", (rider["id"], utc_now()))
    ride_id = db.execute("SELECT last_insert_rowid()").fetchone()[0]
    runtime.update(active_ride_id=ride_id, last_speed=0.0, last_point=None, last_point_time=None)
    return ride_id


def finish_ride(db):
    ride_id = current_ride_id(db)
    if not ride_id:
        return
    ride = row_dict(db.execute("SELECT * FROM rides WHERE id=?", (ride_id,)).fetchone())
    avg = db.execute("SELECT AVG(speed) a FROM ride_points WHERE ride_id=?", (ride_id,)).fetchone()["a"] or 0
    started = parse_time(ride["started_at"])
    duration = max(0, int((datetime.now(timezone.utc) - started).total_seconds())) if started else 0
    ride["average_speed"] = round(avg, 1)
    score, explanation = calculate_safety_score(ride)
    db.execute("UPDATE rides SET ended_at=?,average_speed=?,duration_seconds=?,safety_score=?,score_explanation=? WHERE id=?",
               (utc_now(), round(avg, 1), duration, score, explanation, ride_id))
    runtime.update(active_ride_id=None, last_speed=0.0, last_point=None, last_point_time=None)


def add_event(db, event_type, message, severity="info", lat=None, lon=None):
    ride_id = current_ride_id(db)
    db.execute("INSERT INTO safety_events(ride_id,event_type,severity,message,latitude,longitude,created_at) VALUES(?,?,?,?,?,?,?)",
               (ride_id, event_type, severity, message, lat, lon, utc_now()))
    if ride_id:
        column = {"accident_confirmed": "accident_events", "helmet_removed": "helmet_off_events", "alcohol_detected": "alcohol_events"}.get(event_type)
        if column:
            db.execute(f"UPDATE rides SET {column}={column}+1 WHERE id=?", (ride_id,))


@app.route("/")
def index():
    if "user_id" not in session:
        return redirect(url_for("login"))
    return redirect(url_for("guardian_dashboard" if session["role"] == "guardian" else "rider_dashboard"))


@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        with connect() as db:
            user = db.execute("SELECT * FROM users WHERE lower(email)=lower(?)", (request.form.get("email", ""),)).fetchone()
        if user and check_password_hash(user["password_hash"], request.form.get("password", "")):
            session.clear()
            session.update(user_id=user["id"], name=user["name"], role=user["role"])
            return redirect(url_for("index"))
        flash("Invalid email or password.", "error")
    return render_template("login.html")


@app.route("/logout")
def logout():
    session.clear()
    return redirect(url_for("login"))


@app.route("/rider")
@logged_in
@role_required("rider")
def rider_dashboard():
    return render_template("dashboard_rider.html")


@app.route("/guardian")
@logged_in
@role_required("guardian")
def guardian_dashboard():
    return render_template("dashboard_guardian.html")


@app.route("/rides")
@logged_in
def rides():
    rider_id = session["user_id"]
    if session["role"] == "guardian":
        with connect() as db:
            linked = db.execute("SELECT rider_id FROM users WHERE id=?", (session["user_id"],)).fetchone()
            rider_id = linked["rider_id"]
    with connect() as db:
        items = [dict(x) for x in db.execute("SELECT * FROM rides WHERE rider_id=? ORDER BY id DESC", (rider_id,)).fetchall()]
    return render_template("rides.html", rides=items)


@app.route("/rides/<int:ride_id>")
@logged_in
def ride_detail(ride_id):
    with connect() as db:
        ride = row_dict(db.execute("SELECT * FROM rides WHERE id=?", (ride_id,)).fetchone())
        if not ride:
            abort(404)
        allowed_rider = session["user_id"]
        if session["role"] == "guardian":
            linked = db.execute("SELECT rider_id FROM users WHERE id=?", (session["user_id"],)).fetchone()
            allowed_rider = linked["rider_id"] if linked else None
        if ride["rider_id"] != allowed_rider:
            abort(403)
        points = [dict(x) for x in db.execute("SELECT * FROM ride_points WHERE ride_id=? ORDER BY id", (ride_id,)).fetchall()]
        events = [dict(x) for x in db.execute("SELECT * FROM safety_events WHERE ride_id=? ORDER BY id", (ride_id,)).fetchall()]
    return render_template("ride_detail.html", ride=ride, points=points, events=events)


@app.route("/profile", methods=["GET", "POST"])
@logged_in
def profile():
    with connect() as db:
        if request.method == "POST":
            name = request.form.get("name", "").strip()
            phone = request.form.get("phone", "").strip()
            emergency = request.form.get("emergency_phone", "").strip()
            if not name:
                flash("Name is required.", "error")
            else:
                db.execute("UPDATE users SET name=?,phone=?,emergency_phone=? WHERE id=?", (name, phone, emergency, session["user_id"]))
                if request.form.get("password"):
                    db.execute("UPDATE users SET password_hash=? WHERE id=?", (generate_password_hash(request.form["password"]), session["user_id"]))
                session["name"] = name
                flash("Profile updated.", "success")
        user = row_dict(db.execute("SELECT * FROM users WHERE id=?", (session["user_id"],)).fetchone())
    return render_template("profile.html", user=user)


@app.get("/api/state")
@logged_in
def api_state():
    with connect() as db:
        state = public_state(db.execute("SELECT * FROM device_state WHERE id=1").fetchone())
        state["active_ride_id"] = current_ride_id(db)
        if state["active_ride_id"]:
            ride = row_dict(db.execute("SELECT * FROM rides WHERE id=?", (state["active_ride_id"],)).fetchone())
            score, explanation = calculate_safety_score(ride)
            state["live_score"] = score
            state["score_explanation"] = explanation
        else:
            last = db.execute("SELECT safety_score,score_explanation FROM rides WHERE ended_at IS NOT NULL ORDER BY id DESC LIMIT 1").fetchone()
            state["live_score"] = last["safety_score"] if last else 100
            state["score_explanation"] = last["score_explanation"] if last else "No completed ride yet."
        state["recent_events"] = [dict(x) for x in db.execute("SELECT * FROM safety_events ORDER BY id DESC LIMIT 8").fetchall()]
    return jsonify(state)


@app.post("/api/device/helmet")
@require_device
def helmet_telemetry():
    data = request.get_json(silent=True) or {}
    required = {"helmet_worn", "alcohol_detected", "battery", "accident_status"}
    if not required.issubset(data):
        return jsonify(error="missing fields", required=sorted(required)), 400
    allowed_accident = {"normal", "pending", "cancelled", "confirmed"}
    if data["accident_status"] not in allowed_accident:
        return jsonify(error="invalid accident_status"), 400
    with connect() as db:
        old = row_dict(db.execute("SELECT * FROM device_state WHERE id=1").fetchone())
        deadline = data.get("accident_deadline")
        if data["accident_status"] == "pending" and not deadline:
            from datetime import timedelta
            deadline = old.get("accident_deadline") if old.get("accident_status") == "pending" else None
            deadline = deadline or (datetime.now(timezone.utc) + timedelta(seconds=120)).isoformat()
        db.execute("""UPDATE device_state SET helmet_worn=?,alcohol_detected=?,latitude=?,longitude=?,speed=?,battery=?,
                    accident_status=?,accident_deadline=?,helmet_updated_at=?,last_sms_status=? WHERE id=1""",
                   (bool(data["helmet_worn"]), bool(data["alcohol_detected"]), data.get("latitude"), data.get("longitude"),
                    float(data.get("speed", 0)), float(data["battery"]), data["accident_status"], deadline,
                    utc_now(), data.get("sms_status", old.get("last_sms_status", ""))))
        lat, lon = data.get("latitude"), data.get("longitude")
        if old["helmet_worn"] and not data["helmet_worn"]:
            add_event(db, "helmet_removed", "Helmet removed during active monitoring", "warning", lat, lon)
        if not old["alcohol_detected"] and data["alcohol_detected"]:
            add_event(db, "alcohol_detected", "Alcohol detected; ignition permission denied", "danger", lat, lon)
        if old["accident_status"] != data["accident_status"]:
            status = data["accident_status"]
            severity = "danger" if status == "confirmed" else "warning" if status == "pending" else "info"
            add_event(db, f"accident_{status}", f"Accident alert {status}", severity, lat, lon)
        ride_id = current_ride_id(db)
        if ride_id and lat is not None and lon is not None:
            now = datetime.now(timezone.utc)
            speed = float(data.get("speed", 0))
            db.execute("INSERT INTO ride_points(ride_id,recorded_at,latitude,longitude,speed) VALUES(?,?,?,?,?)", (ride_id, utc_now(), lat, lon, speed))
            distance = 0
            if runtime["last_point"]:
                distance = haversine_km(runtime["last_point"][0], runtime["last_point"][1], float(lat), float(lon))
                if distance > 1:  # reject impossible GPS jumps between samples
                    distance = 0
            accel = speed - runtime["last_speed"]
            harsh_accel = 1 if accel > 12 else 0
            harsh_brake = 1 if accel < -12 else 0
            db.execute("""UPDATE rides SET distance_km=distance_km+?,max_speed=MAX(max_speed,?),
                        harsh_accel_count=harsh_accel_count+?,harsh_brake_count=harsh_brake_count+? WHERE id=?""",
                       (distance, speed, harsh_accel, harsh_brake, ride_id))
            runtime.update(last_point=(float(lat), float(lon)), last_point_time=now, last_speed=speed)
        state = public_state(db.execute("SELECT * FROM device_state WHERE id=1").fetchone())
    socketio.emit("state_update", state)
    return jsonify(ok=True, ignition_allowed=state["ignition_allowed"], server_time=utc_now())


@app.route("/api/device/bike", methods=["GET", "POST"])
@require_device
def bike_api():
    with connect() as db:
        state = public_state(db.execute("SELECT * FROM device_state WHERE id=1").fetchone())
        if request.method == "POST":
            data = request.get_json(silent=True) or {}
            requested_running = bool(data.get("bike_running", False))
            actual_running = requested_running and state["ignition_allowed"]
            was_running = bool(state["bike_running"])
            db.execute("UPDATE device_state SET bike_running=?,ignition_allowed=?,bike_updated_at=? WHERE id=1",
                       (actual_running, state["ignition_allowed"], utc_now()))
            if actual_running and not was_running:
                start_ride(db)
                add_event(db, "ride_started", "Ride started", "info", state.get("latitude"), state.get("longitude"))
            elif was_running and not actual_running:
                add_event(db, "ride_ended", "Ride ended", "info", state.get("latitude"), state.get("longitude"))
                finish_ride(db)
            state["bike_running"] = actual_running
        response = {"ignition_allowed": state["ignition_allowed"], "helmet_online": state["helmet_online"],
                    "helmet_worn": state["helmet_worn"], "alcohol_detected": state["alcohol_detected"],
                    "bike_running": state["bike_running"], "reason": "OK" if state["ignition_allowed"] else "Helmet missing, stale, alcohol detected, or accident alert pending"}
    socketio.emit("state_update", response)
    return jsonify(response)


@app.post("/api/simulator/reset")
@logged_in
def simulator_reset():
    with connect() as db:
        db.execute("UPDATE device_state SET helmet_worn=0,alcohol_detected=0,speed=0,battery=0,accident_status='normal',accident_deadline=NULL,bike_running=0,ignition_allowed=0,helmet_updated_at=NULL,bike_updated_at=NULL WHERE id=1")
        finish_ride(db)
    socketio.emit("state_update", {"reset": True})
    return jsonify(ok=True)


@app.errorhandler(403)
def forbidden(_):
    return render_template("error.html", code=403, message="You do not have access to this page."), 403


@app.errorhandler(404)
def not_found(_):
    return render_template("error.html", code=404, message="Page not found."), 404


def lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return "127.0.0.1"


def main():
    init_db()
    print("\nSmartRide is starting...")
    print(f"This PC: http://127.0.0.1:{config.PORT}")
    print(f"Other devices on Wi-Fi: http://{lan_ip()}:{config.PORT}")
    if os.getenv("SMARTRIDE_NO_BROWSER") != "1":
        threading.Timer(1.5, lambda: webbrowser.open(f"http://127.0.0.1:{config.PORT}")).start()
    socketio.run(app, host=config.HOST, port=config.PORT, allow_unsafe_werkzeug=True)


init_db()

if __name__ == "__main__":
    main()
