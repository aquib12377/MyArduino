# SmartRide Working Flow

## Before the ride

1. Helmet switch reports whether the strap/helmet is worn.
2. MQ-3 reports the calibrated alcohol state.
3. The single ESP32 evaluates `helmet worn + no alcohol` locally.
4. Only when both checks pass does the start/stop button enable the prototype motor.
5. Flask independently checks fresh telemetry for dashboard status and ride recording.

## During the ride

1. The single ESP32 reads all sensors and controls the prototype motor.
2. It posts helmet telemetry and bike state to Flask every second.
3. The first reported bike start creates a ride record.
4. Valid GPS points store route, speed and incremental distance.
5. Large speed changes increment sudden acceleration/braking counters.
6. Socket updates and two-second browser polling keep both dashboards current.
7. If Wi-Fi/server connectivity is lost, local helmet and alcohol safety logic continues; dashboard storage resumes after reconnection.

## Accident flow

1. Only during an active ride, MPU6050 impact (2.5 g) or tilt (40 degrees held for one second) triggers `pending` and stops the prototype motor. Mount the MPU6050 with its Z axis pointing up. These thresholds require bench calibration.
2. Buzzer sounds and a 120-second local helmet timer begins.
3. Website shows the same deadline to rider and guardian.
4. Short press on the physical cancel button sets `cancelled` and prevents SMS.
5. If time expires, the helmet obtains the latest GPS coordinates and sends a Google Maps link through SIM800L.
6. Helmet reports `confirmed` and SMS state to Flask.
7. Guardian dashboard highlights the emergency and Flask stores the location/event.
8. After cancellation or confirmation, press Start to begin another ride and automatically re-arm monitoring. Helmet/alcohol checks still apply. Start is blocked while an alert is pending; no long press or separate reset is needed.

## After the ride

1. The start button toggles the bike off.
2. Flask closes the active ride.
3. Average speed and the final rule-based safety score are calculated.
4. Rider and guardian can open the route, statistics, explanation and events in Ride History.

## API summary

| Method | Route | Caller | Purpose |
|---|---|---|---|
| POST | `/api/device/helmet` | Single ESP32 | Sensor, GPS and accident telemetry |
| POST | `/api/device/bike` | Single ESP32 | Report locally controlled bike state |
| GET | `/api/state` | Authenticated dashboard | Current state, score and events |

Device routes require the `X-Device-Key` header. Browser routes require a logged-in session.
