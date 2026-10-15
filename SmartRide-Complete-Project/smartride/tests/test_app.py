import os
import tempfile
import unittest

db_file = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
db_file.close()
os.environ["SMARTRIDE_DATABASE"] = db_file.name
os.environ["SMARTRIDE_SECRET_KEY"] = "test-secret"

from app import app  # noqa: E402
from scoring import calculate_safety_score  # noqa: E402


class SmartRideTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        try:
            os.unlink(db_file.name)
        except OSError:
            pass

    def setUp(self):
        app.config.update(TESTING=True)
        self.client = app.test_client()

    def login(self, email="rider@smartride.local", password="Rider@123"):
        return self.client.post("/login", data={"email": email, "password": password})

    def test_rider_login(self):
        response = self.login()
        self.assertEqual(response.status_code, 302)
        self.assertIn("/", response.location)

    def test_guardian_cannot_open_rider_dashboard(self):
        self.login("guardian@smartride.local", "Guardian@123")
        self.assertEqual(self.client.get("/rider").status_code, 403)

    def test_device_key_is_required(self):
        response = self.client.post("/api/device/helmet", json={})
        self.assertEqual(response.status_code, 401)

    def test_helmet_permission_and_bike_start(self):
        headers = {"X-Device-Key": "smartride-demo-device-key"}
        payload = {"helmet_worn": True, "alcohol_detected": False, "battery": 90,
                   "accident_status": "normal", "latitude": 19.0, "longitude": 72.8, "speed": 0}
        helmet = self.client.post("/api/device/helmet", json=payload, headers=headers)
        self.assertTrue(helmet.get_json()["ignition_allowed"])
        bike = self.client.post("/api/device/bike", json={"bike_running": True}, headers=headers)
        self.assertTrue(bike.get_json()["bike_running"])
        self.client.post("/api/device/bike", json={"bike_running": False}, headers=headers)

    def test_alcohol_denies_permission(self):
        headers = {"X-Device-Key": "smartride-demo-device-key"}
        payload = {"helmet_worn": True, "alcohol_detected": True, "battery": 90,
                   "accident_status": "normal", "latitude": 19.0, "longitude": 72.8, "speed": 0}
        response = self.client.post("/api/device/helmet", json=payload, headers=headers)
        self.assertFalse(response.get_json()["ignition_allowed"])

    def test_pending_alert_blocks_start_and_cancel_restores_permission(self):
        headers = {"X-Device-Key": "smartride-demo-device-key"}
        payload = {"helmet_worn": True, "alcohol_detected": False, "battery": 90,
                   "accident_status": "pending"}
        response = self.client.post("/api/device/helmet", json=payload, headers=headers)
        self.assertFalse(response.get_json()["ignition_allowed"])
        response = self.client.post("/api/device/bike", json={"bike_running": True}, headers=headers)
        self.assertFalse(response.get_json()["bike_running"])
        for status in ("cancelled", "confirmed"):
            payload["accident_status"] = status
            response = self.client.post("/api/device/helmet", json=payload, headers=headers)
            self.assertTrue(response.get_json()["ignition_allowed"])
        payload["accident_status"] = "normal"
        self.client.post("/api/device/helmet", json=payload, headers=headers)

    def test_score_deductions(self):
        score, explanation = calculate_safety_score({"max_speed": 90, "harsh_accel_count": 2, "harsh_brake_count": 1})
        self.assertLess(score, 100)
        self.assertIn("sudden acceleration", explanation)


if __name__ == "__main__":
    unittest.main()

