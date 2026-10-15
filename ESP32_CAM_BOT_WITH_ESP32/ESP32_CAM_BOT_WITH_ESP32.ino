#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

// =========================================================
// WIFI
// =========================================================
const char* WIFI_SSID = "AAA";
const char* WIFI_PASSWORD = "12345678";

// =========================================================
// SENSOR PINS
// =========================================================
#define MQ135_PIN       34

#define DHT_PIN         4
#define DHT_TYPE        DHT11

#define TRIG_PIN        5
#define ECHO_PIN        18

// =========================================================
// L293D MOTOR DRIVER
// =========================================================
#define MOTOR_L_IN1     25
#define MOTOR_L_IN2     26

#define MOTOR_R_IN1     27
#define MOTOR_R_IN2     14

// EN1 and EN2 are permanently connected to 5V.

// =========================================================
// SETTINGS
// =========================================================
#define OBSTACLE_DISTANCE_CM  20.0

// Stops robot if WebSocket control packets disappear.
#define MOTOR_COMMAND_TIMEOUT 800

// Allows reverse/turn after an obstacle stopped the robot.
// Forward remains blocked.
#define ALLOW_ESCAPE_MOVEMENT true

// =========================================================
// OBJECTS
// =========================================================
DHT dht(DHT_PIN, DHT_TYPE);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// =========================================================
// SENSOR VALUES
// =========================================================
float temperature = 0;
float humidity = 0;
float distanceCm = 999;

int mq135Raw = 0;
float mq135Voltage = 0;

bool obstacleDetected = false;

// =========================================================
// MOTOR STATE
// =========================================================
enum RobotDirection {
  ROBOT_STOP,
  ROBOT_FORWARD,
  ROBOT_BACKWARD,
  ROBOT_LEFT,
  ROBOT_RIGHT
};

RobotDirection robotDirection = ROBOT_STOP;

unsigned long lastMotorCommand = 0;

// =========================================================
// TIMERS
// =========================================================
unsigned long lastDHTRead = 0;
unsigned long lastMQRead = 0;
unsigned long lastUltrasonicRead = 0;
unsigned long lastTelemetry = 0;

// =========================================================
// WEB PAGE
// =========================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta
        name="viewport"
        content="width=device-width, initial-scale=1.0">

    <title>ESP32 Environmental Robot</title>

    <style>
        :root {
            --bg: #0f172a;
            --panel: #172033;
            --panel2: #1e293b;
            --border: #334155;
            --text: #f8fafc;
            --muted: #94a3b8;
            --accent: #3b82f6;
            --accent2: #2563eb;
            --green: #22c55e;
            --red: #ef4444;
            --amber: #f59e0b;
        }

        * {
            box-sizing: border-box;
        }

        body {
            margin: 0;
            font-family:
                Inter,
                -apple-system,
                BlinkMacSystemFont,
                "Segoe UI",
                sans-serif;

            background:
                radial-gradient(
                    circle at top,
                    #172554 0,
                    var(--bg) 35%,
                    #020617 100%
                );

            color: var(--text);
            min-height: 100vh;
        }

        .container {
            width: min(1250px, 94%);
            margin: auto;
            padding: 25px 0 40px;
        }

        header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 20px;
            margin-bottom: 24px;
        }

        .title h1 {
            margin: 0;
            font-size: clamp(22px, 4vw, 32px);
            font-weight: 700;
        }

        .title p {
            margin: 5px 0 0;
            color: var(--muted);
            font-size: 14px;
        }

        .connection {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 9px 14px;
            border-radius: 50px;
            border: 1px solid var(--border);
            background: rgba(30, 41, 59, .85);
            font-size: 13px;
        }

        .dot {
            width: 9px;
            height: 9px;
            border-radius: 50%;
            background: var(--amber);
        }

        .dot.connected {
            background: var(--green);
            box-shadow: 0 0 10px rgba(34, 197, 94, .6);
        }

        .grid {
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 14px;
        }

        .card {
            background: rgba(23, 32, 51, .91);
            border: 1px solid rgba(71, 85, 105, .55);
            border-radius: 16px;
            padding: 18px;
            box-shadow: 0 10px 35px rgba(0,0,0,.15);
        }

        .card-label {
            color: var(--muted);
            font-size: 13px;
            margin-bottom: 8px;
        }

        .value {
            font-size: 30px;
            font-weight: 700;
        }

        .unit {
            color: var(--muted);
            font-size: 14px;
        }

        .subvalue {
            color: var(--muted);
            margin-top: 7px;
            font-size: 12px;
        }

        .main-grid {
            display: grid;
            grid-template-columns: 1.6fr .85fr;
            gap: 18px;
            margin-top: 18px;
        }

        .section-title {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 15px;
        }

        .section-title h2 {
            margin: 0;
            font-size: 17px;
        }

        /* ================= CAMERA ================= */

        .camera-frame {
            background: #020617;
            width: 100%;
            aspect-ratio: 4 / 3;
            border-radius: 12px;
            overflow: hidden;
            display: flex;
            align-items: center;
            justify-content: center;
            border: 1px solid var(--border);
        }

        .camera-frame img {
            width: 100%;
            height: 100%;
            object-fit: contain;
            display: none;
        }

        #cameraPlaceholder {
            text-align: center;
            color: var(--muted);
            padding: 30px;
        }

        .camera-controls {
            display: flex;
            gap: 8px;
            margin-top: 12px;
        }

        input {
            flex: 1;
            min-width: 0;
            background: #0f172a;
            color: var(--text);
            border: 1px solid var(--border);
            border-radius: 9px;
            padding: 11px 12px;
            outline: none;
        }

        input:focus {
            border-color: var(--accent);
        }

        .normal-btn {
            border: none;
            color: white;
            background: var(--accent);
            border-radius: 9px;
            padding: 0 18px;
            cursor: pointer;
            font-weight: 600;
        }

        .normal-btn:hover {
            background: var(--accent2);
        }

        /* ================= ROBOT CONTROLS ================= */

        .robot-status {
            text-align: center;
            margin-bottom: 18px;
        }

        .robot-state {
            font-size: 24px;
            font-weight: 700;
        }

        .obstacle-box {
            margin-top: 10px;
            padding: 10px;
            border-radius: 8px;
            background: rgba(34,197,94,.10);
            color: #86efac;
            border: 1px solid rgba(34,197,94,.25);
            font-size: 13px;
        }

        .obstacle-box.warning {
            background: rgba(239,68,68,.12);
            border-color: rgba(239,68,68,.35);
            color: #fca5a5;
        }

        .drive-pad {
            width: 235px;
            margin: auto;
            display: grid;
            grid-template-columns: repeat(3, 68px);
            grid-template-rows: repeat(3, 68px);
            gap: 9px;
            justify-content: center;
        }

        .drive-btn {
            background: #263449;
            color: white;
            border: 1px solid #3f4d64;
            border-radius: 14px;
            cursor: pointer;
            font-size: 24px;
            user-select: none;
            touch-action: none;
            transition: .12s;
        }

        .drive-btn:hover {
            background: #334155;
            border-color: #64748b;
        }

        .drive-btn:active,
        .drive-btn.active {
            background: var(--accent);
            transform: scale(.96);
        }

        .forward {
            grid-column: 2;
            grid-row: 1;
        }

        .left {
            grid-column: 1;
            grid-row: 2;
        }

        .stop {
            grid-column: 2;
            grid-row: 2;
            background: #7f1d1d;
            border-color: #991b1b;
        }

        .right {
            grid-column: 3;
            grid-row: 2;
        }

        .backward {
            grid-column: 2;
            grid-row: 3;
        }

        .hint {
            margin-top: 18px;
            text-align: center;
            font-size: 12px;
            color: var(--muted);
            line-height: 1.6;
        }

        @media(max-width: 850px) {
            .grid {
                grid-template-columns: repeat(2, 1fr);
            }

            .main-grid {
                grid-template-columns: 1fr;
            }
        }

        @media(max-width: 500px) {
            .container {
                width: 92%;
            }

            header {
                align-items: flex-start;
                flex-direction: column;
            }

            .grid {
                grid-template-columns: repeat(2, 1fr);
            }

            .card {
                padding: 14px;
            }

            .value {
                font-size: 24px;
            }

            .camera-controls {
                flex-direction: column;
            }

            .normal-btn {
                padding: 11px;
            }
        }
    </style>
</head>

<body>

<div class="container">

    <header>
        <div class="title">
            <h1>Environmental Robot</h1>
            <p>ESP32 Robot Control & Environmental Monitoring</p>
        </div>

        <div class="connection">
            <span
                class="dot"
                id="connectionDot">
            </span>

            <span id="connectionText">
                Connecting
            </span>
        </div>
    </header>

    <!-- SENSOR CARDS -->

    <div class="grid">

        <div class="card">
            <div class="card-label">
                Temperature
            </div>

            <span
                class="value"
                id="temperature">
                --
            </span>

            <span class="unit">°C</span>
        </div>

        <div class="card">
            <div class="card-label">
                Humidity
            </div>

            <span
                class="value"
                id="humidity">
                --
            </span>

            <span class="unit">%</span>
        </div>

        <div class="card">
            <div class="card-label">
                MQ135 Gas
            </div>

            <span
                class="value"
                id="gas">
                --
            </span>

            <span class="unit">
                ADC
            </span>

            <div
                class="subvalue"
                id="gasVoltage">
                --
            </div>
        </div>

        <div class="card">
            <div class="card-label">
                Front Distance
            </div>

            <span
                class="value"
                id="distance">
                --
            </span>

            <span class="unit">
                cm
            </span>
        </div>

    </div>

    <div class="main-grid">

        <!-- CAMERA -->

        <div class="card">

            <div class="section-title">
                <h2>Live Camera</h2>
            </div>

            <div class="camera-frame">

                <div id="cameraPlaceholder">
                    Enter ESP32-CAM IP below
                </div>

                <img
                    id="cameraStream"
                    alt="ESP32 Camera Stream">
            </div>

            <div class="camera-controls">

                <input
                    id="cameraIP"
                    placeholder="Example: 192.168.1.120">

                <button
                    class="normal-btn"
                    onclick="loadCamera()">
                    Connect Camera
                </button>

            </div>

        </div>

        <!-- ROBOT -->

        <div class="card">

            <div class="section-title">
                <h2>Robot Control</h2>
            </div>

            <div class="robot-status">

                <div
                    class="robot-state"
                    id="robotState">
                    STOPPED
                </div>

                <div
                    class="obstacle-box"
                    id="obstacleBox">
                    Path clear
                </div>

            </div>

            <div class="drive-pad">

                <button
                    class="drive-btn forward"
                    data-command="F">
                    ▲
                </button>

                <button
                    class="drive-btn left"
                    data-command="L">
                    ◀
                </button>

                <button
                    class="drive-btn stop"
                    onclick="stopDrive()">
                    ■
                </button>

                <button
                    class="drive-btn right"
                    data-command="R">
                    ▶
                </button>

                <button
                    class="drive-btn backward"
                    data-command="B">
                    ▼
                </button>

            </div>

            <div class="hint">
                Hold a direction button to move.<br>
                Keyboard: W/A/S/D or arrow keys.
            </div>

        </div>

    </div>

</div>

<script>

let websocket;

let driveTimer = null;

let currentCommand = null;

// =====================================================
// WEBSOCKET
// =====================================================

function connectWebSocket() {

    websocket = new WebSocket(
        `ws://${window.location.host}/ws`
    );

    websocket.onopen = function() {

        document
            .getElementById("connectionDot")
            .classList.add("connected");

        document
            .getElementById("connectionText")
            .innerText = "Connected";
    };

    websocket.onclose = function() {

        document
            .getElementById("connectionDot")
            .classList.remove("connected");

        document
            .getElementById("connectionText")
            .innerText = "Disconnected";

        clearInterval(driveTimer);

        setTimeout(
            connectWebSocket,
            1500
        );
    };

    websocket.onmessage = function(event) {

        try {

            const data =
                JSON.parse(event.data);

            if(data.type !== "telemetry")
                return;

            document
                .getElementById("temperature")
                .innerText =
                Number(data.temperature).toFixed(1);

            document
                .getElementById("humidity")
                .innerText =
                Number(data.humidity).toFixed(1);

            document
                .getElementById("gas")
                .innerText =
                data.gasRaw;

            document
                .getElementById("gasVoltage")
                .innerText =
                "Estimated sensor output: " +
                Number(data.gasVoltage).toFixed(2) +
                " V";

            if(data.distance > 400) {

                document
                    .getElementById("distance")
                    .innerText = "--";

            } else {

                document
                    .getElementById("distance")
                    .innerText =
                    Number(data.distance).toFixed(1);
            }

            document
                .getElementById("robotState")
                .innerText =
                data.direction;

            const box =
                document.getElementById(
                    "obstacleBox"
                );

            if(data.obstacle) {

                box.classList.add("warning");

                box.innerText =
                    "Obstacle detected — forward locked";

            } else {

                box.classList.remove("warning");

                box.innerText =
                    "Path clear";
            }

        }
        catch(error) {

            console.log(
                "Invalid telemetry",
                error
            );
        }
    };
}

function sendCommand(command) {

    if(
        websocket &&
        websocket.readyState ===
        WebSocket.OPEN
    ) {
        websocket.send(command);
    }
}

// =====================================================
// DRIVE
// =====================================================

function startDrive(command) {

    currentCommand = command;

    sendCommand(command);

    clearInterval(driveTimer);

    driveTimer =
        setInterval(
            () => sendCommand(command),
            250
        );
}

function stopDrive() {

    currentCommand = null;

    clearInterval(driveTimer);

    driveTimer = null;

    sendCommand("STOP");

    document
        .querySelectorAll(".drive-btn")
        .forEach(
            button =>
                button.classList.remove(
                    "active"
                )
        );
}

// Touch / Mouse

document
    .querySelectorAll(
        "[data-command]"
    )
    .forEach(button => {

        const command =
            button.dataset.command;

        button.addEventListener(
            "pointerdown",
            event => {

                event.preventDefault();

                button.classList.add(
                    "active"
                );

                startDrive(command);
            }
        );

        button.addEventListener(
            "pointerup",
            stopDrive
        );

        button.addEventListener(
            "pointercancel",
            stopDrive
        );

        button.addEventListener(
            "pointerleave",
            stopDrive
        );
    });

// =====================================================
// KEYBOARD
// =====================================================

const keyMap = {

    "w": "F",
    "ArrowUp": "F",

    "s": "B",
    "ArrowDown": "B",

    "a": "L",
    "ArrowLeft": "L",

    "d": "R",
    "ArrowRight": "R"
};

document.addEventListener(
    "keydown",
    event => {

        if(event.repeat)
            return;

        const command =
            keyMap[event.key];

        if(command) {

            event.preventDefault();

            startDrive(command);
        }
    }
);

document.addEventListener(
    "keyup",
    event => {

        if(keyMap[event.key]) {

            event.preventDefault();

            stopDrive();
        }
    }
);

// =====================================================
// CAMERA
// =====================================================

function loadCamera() {

    let ip =
        document
            .getElementById(
                "cameraIP"
            )
            .value
            .trim();

    if(!ip)
        return;

    ip =
        ip
            .replace(
                /^https?:\/\//,
                ""
            )
            .replace(
                /\/.*$/,
                ""
            );

    localStorage.setItem(
        "esp32CameraIP",
        ip
    );

    const image =
        document.getElementById(
            "cameraStream"
        );

    const placeholder =
        document.getElementById(
            "cameraPlaceholder"
        );

    placeholder.style.display =
        "none";

    image.style.display =
        "block";

    image.src =
        "http://" +
        ip +
        "/stream?t=" +
        Date.now();
}

window.addEventListener(
    "load",
    () => {

        connectWebSocket();

        const savedIP =
            localStorage.getItem(
                "esp32CameraIP"
            );

        if(savedIP) {

            document
                .getElementById(
                    "cameraIP"
                )
                .value =
                savedIP;

            loadCamera();
        }
    }
);

// Make sure robot stops if tab loses focus

window.addEventListener(
    "blur",
    stopDrive
);

</script>

</body>
</html>
)rawliteral";

// =========================================================
// MOTOR FUNCTIONS
// =========================================================

void stopMotors()
{
    digitalWrite(MOTOR_L_IN1, LOW);
    digitalWrite(MOTOR_L_IN2, LOW);

    digitalWrite(MOTOR_R_IN1, LOW);
    digitalWrite(MOTOR_R_IN2, LOW);

    robotDirection = ROBOT_STOP;
}

void moveForward()
{
    // Left motor
    digitalWrite(MOTOR_L_IN1, HIGH);
    digitalWrite(MOTOR_L_IN2, LOW);

    // Right motor
    digitalWrite(MOTOR_R_IN1, HIGH);
    digitalWrite(MOTOR_R_IN2, LOW);

    robotDirection = ROBOT_FORWARD;
}

void moveBackward()
{
    digitalWrite(MOTOR_L_IN1, LOW);
    digitalWrite(MOTOR_L_IN2, HIGH);

    digitalWrite(MOTOR_R_IN1, LOW);
    digitalWrite(MOTOR_R_IN2, HIGH);

    robotDirection = ROBOT_BACKWARD;
}

void turnLeft()
{
    // Pivot left

    digitalWrite(MOTOR_L_IN1, LOW);
    digitalWrite(MOTOR_L_IN2, HIGH);

    digitalWrite(MOTOR_R_IN1, HIGH);
    digitalWrite(MOTOR_R_IN2, LOW);

    robotDirection = ROBOT_LEFT;
}

void turnRight()
{
    // Pivot right

    digitalWrite(MOTOR_L_IN1, HIGH);
    digitalWrite(MOTOR_L_IN2, LOW);

    digitalWrite(MOTOR_R_IN1, LOW);
    digitalWrite(MOTOR_R_IN2, HIGH);

    robotDirection = ROBOT_RIGHT;
}

// =========================================================
// DIRECTION TEXT
// =========================================================

const char* directionText()
{
    switch(robotDirection)
    {
        case ROBOT_FORWARD:
            return "FORWARD";

        case ROBOT_BACKWARD:
            return "BACKWARD";

        case ROBOT_LEFT:
            return "LEFT";

        case ROBOT_RIGHT:
            return "RIGHT";

        default:
            return "STOPPED";
    }
}

// =========================================================
// ULTRASONIC
// =========================================================

float readDistance()
{
    digitalWrite(
        TRIG_PIN,
        LOW
    );

    delayMicroseconds(3);

    digitalWrite(
        TRIG_PIN,
        HIGH
    );

    delayMicroseconds(10);

    digitalWrite(
        TRIG_PIN,
        LOW
    );

    unsigned long duration =
        pulseIn(
            ECHO_PIN,
            HIGH,
            25000
        );

    if(duration == 0)
        return 999.0;

    return duration * 0.0343 / 2.0;
}

// =========================================================
// MQ135 AVERAGE
// =========================================================

int readMQ135Average()
{
    long total = 0;

    const int samples = 12;

    for(int i = 0; i < samples; i++)
    {
        total +=
            analogRead(
                MQ135_PIN
            );

        delayMicroseconds(200);
    }

    return total / samples;
}

// =========================================================
// TELEMETRY
// =========================================================

void sendTelemetry()
{
    char json[300];

    snprintf(
        json,
        sizeof(json),

        "{"
        "\"type\":\"telemetry\","
        "\"temperature\":%.1f,"
        "\"humidity\":%.1f,"
        "\"gasRaw\":%d,"
        "\"gasVoltage\":%.2f,"
        "\"distance\":%.1f,"
        "\"obstacle\":%s,"
        "\"direction\":\"%s\""
        "}",

        temperature,
        humidity,

        mq135Raw,
        mq135Voltage,

        distanceCm,

        obstacleDetected
            ? "true"
            : "false",

        directionText()
    );

    ws.textAll(json);
}

// =========================================================
// COMMAND HANDLING
// =========================================================

void handleRobotCommand(
    const String &command
)
{
    lastMotorCommand =
        millis();

    if(command == "STOP")
    {
        stopMotors();
        return;
    }

    // -----------------------------
    // Obstacle protection
    // -----------------------------

    if(obstacleDetected)
    {
        // Always prevent forward movement

        if(command == "F")
        {
            stopMotors();
            return;
        }

        if(!ALLOW_ESCAPE_MOVEMENT)
        {
            stopMotors();
            return;
        }
    }

    if(command == "F")
    {
        moveForward();
    }

    else if(command == "B")
    {
        moveBackward();
    }

    else if(command == "L")
    {
        turnLeft();
    }

    else if(command == "R")
    {
        turnRight();
    }
}

// =========================================================
// WEBSOCKET EVENT
// =========================================================

void onWebSocketEvent(
    AsyncWebSocket *server,
    AsyncWebSocketClient *client,
    AwsEventType type,
    void *arg,
    uint8_t *data,
    size_t len
)
{
    if(type == WS_EVT_CONNECT)
    {
        Serial.print(
            "WebSocket client connected: "
        );

        Serial.println(
            client->id()
        );

        return;
    }

    if(type == WS_EVT_DISCONNECT)
    {
        Serial.print(
            "WebSocket client disconnected: "
        );

        Serial.println(
            client->id()
        );

        // Immediate safety stop
        stopMotors();

        return;
    }

    if(type == WS_EVT_DATA)
    {
        AwsFrameInfo *info =
            (AwsFrameInfo*)arg;

        if(
            info->final &&
            info->index == 0 &&
            info->len == len &&
            info->opcode == WS_TEXT
        )
        {
            String message;

            message.reserve(len);

            for(size_t i = 0; i < len; i++)
            {
                message +=
                    (char)data[i];
            }

            Serial.print(
                "Command: "
            );

            Serial.println(
                message
            );

            handleRobotCommand(
                message
            );
        }
    }
}

// =========================================================
// WIFI
// =========================================================

void connectWiFi()
{
    Serial.print(
        "Connecting to WiFi"
    );

    WiFi.mode(
        WIFI_STA
    );

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    while(
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println(
        "WiFi connected"
    );

    Serial.print(
        "Robot webpage: http://"
    );

    Serial.println(
        WiFi.localIP()
    );
}

// =========================================================
// SETUP
// =========================================================

void setup()
{
    Serial.begin(115200);

    // --------------------------
    // Motor
    // --------------------------

    pinMode(
        MOTOR_L_IN1,
        OUTPUT
    );

    pinMode(
        MOTOR_L_IN2,
        OUTPUT
    );

    pinMode(
        MOTOR_R_IN1,
        OUTPUT
    );

    pinMode(
        MOTOR_R_IN2,
        OUTPUT
    );

    stopMotors();

    // --------------------------
    // Ultrasonic
    // --------------------------

    pinMode(
        TRIG_PIN,
        OUTPUT
    );

    pinMode(
        ECHO_PIN,
        INPUT
    );

    digitalWrite(
        TRIG_PIN,
        LOW
    );

    // --------------------------
    // MQ135
    // --------------------------

    pinMode(
        MQ135_PIN,
        INPUT
    );

    analogReadResolution(
        12
    );

    // --------------------------
    // DHT
    // --------------------------

    dht.begin();

    // --------------------------
    // WiFi
    // --------------------------

    connectWiFi();

    // --------------------------
    // WebSocket
    // --------------------------

    ws.onEvent(
        onWebSocketEvent
    );

    server.addHandler(
        &ws
    );

    // --------------------------
    // Webpage
    // --------------------------

    server.on(
        "/",
        HTTP_GET,

        [](
            AsyncWebServerRequest *request
        )
        {
            request->send_P(
                200,
                "text/html",
                index_html
            );
        }
    );

    server.begin();

    Serial.println(
        "HTTP server started."
    );
}

// =========================================================
// LOOP
// =========================================================

void loop()
{
    unsigned long now =
        millis();

    // =====================================================
    // MQ135
    // =====================================================

    if(
        now - lastMQRead >=
        300
    )
    {
        lastMQRead = now;

        mq135Raw =
            readMQ135Average();

        // Approximate voltage present at the ESP32 ADC pin

        float adcVoltage =
            (
                mq135Raw /
                4095.0
            ) * 3.3;

        // Compensate for the 10K / 20K divider.
        //
        // Divider ratio:
        //
        // Vesp = Vin × 20 / 30
        //
        // Therefore:
        //
        // Vin = Vesp × 1.5

        mq135Voltage =
            adcVoltage * 1.5;
    }

    // =====================================================
    // DHT11
    // =====================================================

    if(
        now - lastDHTRead >=
        2200
    )
    {
        lastDHTRead = now;

        float newHumidity =
            dht.readHumidity();

        float newTemperature =
            dht.readTemperature();

        if(
            !isnan(newHumidity) &&
            !isnan(newTemperature)
        )
        {
            humidity =
                newHumidity;

            temperature =
                newTemperature;
        }
        else
        {
            Serial.println(
                "DHT11 read failed."
            );
        }
    }

    // =====================================================
    // ULTRASONIC
    // =====================================================

    if(
        now - lastUltrasonicRead >=
        100
    )
    {
        lastUltrasonicRead = now;

        distanceCm =
            readDistance();

        bool newObstacle =
            (
                distanceCm > 0 &&
                distanceCm <=
                OBSTACLE_DISTANCE_CM
            );

        // Obstacle just appeared
        if(
            newObstacle &&
            !obstacleDetected
        )
        {
            Serial.println(
                "OBSTACLE DETECTED - STOP"
            );

            stopMotors();
        }

        obstacleDetected =
            newObstacle;

        // Extra protection:
        // even while obstacle remains,
        // forward movement must stop.

        if(
            obstacleDetected &&
            robotDirection ==
            ROBOT_FORWARD
        )
        {
            stopMotors();
        }
    }

    // =====================================================
    // MOTOR DEAD-MAN SAFETY
    // =====================================================

    if(
        robotDirection !=
        ROBOT_STOP
    )
    {
        if(
            now -
            lastMotorCommand >
            MOTOR_COMMAND_TIMEOUT
        )
        {
            stopMotors();

            Serial.println(
                "Control timeout - STOP"
            );
        }
    }

    // =====================================================
    // TELEMETRY
    // =====================================================

    if(
        now - lastTelemetry >=
        400
    )
    {
        lastTelemetry = now;

        sendTelemetry();
    }

    // =====================================================
    // WEBSOCKET CLEANUP
    // =====================================================

    ws.cleanupClients();
}