#include "esp_camera.h"
#include "img_converters.h"

#include <WiFi.h>
#include "esp_http_server.h"

// =========================================================
// WIFI
// =========================================================

const char* WIFI_SSID =
    "AAA";

const char* WIFI_PASSWORD =
    "12345678";

// =========================================================
// AI THINKER ESP32-CAM PIN MAP
// =========================================================
//
// This pin layout is for the common AI-Thinker-style ESP32-CAM
// board carrying the RHYX M21-45 camera.
//

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1

#define XCLK_GPIO_NUM      0

#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// =========================================================
// CAMERA SETTINGS
// =========================================================

// Best starting point for RHYX M21-45 / GC2145-class sensor.
//
// 320x240 RGB565 requires:
//
// 320 × 240 × 2 = 153,600 bytes/frame
//
// Keep only ONE framebuffer on classic ESP32.

#define CAMERA_FRAME_SIZE    FRAMESIZE_QVGA

// frame2jpg() quality: 0-100.
// 50-60 is a good compromise for this sensor.
//
// Higher = better image but slower/larger stream.

#define SOFTWARE_JPEG_QUALITY 55

// =========================================================
// HTTP SERVER
// =========================================================

httpd_handle_t cameraServer = NULL;

// MJPEG boundary

#define PART_BOUNDARY \
    "123456789000000000000987654321"

static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary="
    PART_BOUNDARY;

static const char* STREAM_BOUNDARY =
    "\r\n--"
    PART_BOUNDARY
    "\r\n";

static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n"
    "\r\n";

// =========================================================
// STREAM HANDLER
// =========================================================

static esp_err_t streamHandler(
    httpd_req_t *request
)
{
    esp_err_t result =
        httpd_resp_set_type(
            request,
            STREAM_CONTENT_TYPE
        );

    if(result != ESP_OK)
        return result;

    httpd_resp_set_hdr(
        request,
        "Access-Control-Allow-Origin",
        "*"
    );

    while(true)
    {
        camera_fb_t *frame =
            esp_camera_fb_get();

        if(!frame)
        {
            Serial.println(
                "Camera capture failed"
            );

            return ESP_FAIL;
        }

        uint8_t *jpegBuffer =
            NULL;

        size_t jpegLength =
            0;

        bool converted =
            false;

        // RHYX / GC2145 gives RGB565.
        //
        // Convert RGB565 -> JPEG in software.

        if(
            frame->format ==
            PIXFORMAT_JPEG
        )
        {
            jpegBuffer =
                frame->buf;

            jpegLength =
                frame->len;
        }
        else
        {
            converted =
                frame2jpg(
                    frame,
                    SOFTWARE_JPEG_QUALITY,
                    &jpegBuffer,
                    &jpegLength
                );

            if(!converted)
            {
                Serial.println(
                    "JPEG conversion failed"
                );

                esp_camera_fb_return(
                    frame
                );

                continue;
            }
        }

        // -------------------------------
        // Boundary
        // -------------------------------

        result =
            httpd_resp_send_chunk(
                request,
                STREAM_BOUNDARY,
                strlen(
                    STREAM_BOUNDARY
                )
            );

        // -------------------------------
        // JPEG header
        // -------------------------------

        if(result == ESP_OK)
        {
            char header[80];

            int headerLength =
                snprintf(
                    header,
                    sizeof(header),
                    STREAM_PART,
                    jpegLength
                );

            result =
                httpd_resp_send_chunk(
                    request,
                    header,
                    headerLength
                );
        }

        // -------------------------------
        // JPEG frame
        // -------------------------------

        if(result == ESP_OK)
        {
            result =
                httpd_resp_send_chunk(
                    request,
                    (const char*)
                    jpegBuffer,
                    jpegLength
                );
        }

        // -------------------------------
        // Release memory
        // -------------------------------

        if(
            frame->format !=
            PIXFORMAT_JPEG &&
            jpegBuffer
        )
        {
            free(
                jpegBuffer
            );

            jpegBuffer =
                NULL;
        }

        esp_camera_fb_return(
            frame
        );

        if(result != ESP_OK)
        {
            Serial.println(
                "Client disconnected."
            );

            break;
        }

        // Give WiFi/RTOS a tiny opportunity to run.
        vTaskDelay(
            1 /
            portTICK_PERIOD_MS
        );
    }

    return result;
}

// =========================================================
// ROOT PAGE
// =========================================================

static esp_err_t indexHandler(
    httpd_req_t *request
)
{
    const char webpage[] =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta name='viewport' "
        "content='width=device-width,initial-scale=1'>"
        "<style>"
        "body{"
        "background:#0f172a;"
        "color:white;"
        "font-family:Arial;"
        "text-align:center;"
        "margin:0;"
        "padding:25px;"
        "}"
        "img{"
        "width:min(640px,100%);"
        "border-radius:12px;"
        "background:#000;"
        "}"
        "p{color:#94a3b8;}"
        "</style>"
        "</head>"
        "<body>"
        "<h2>ESP32-CAM</h2>"
        "<p>RHYX M21-45 RGB565 Stream</p>"
        "<img src='/stream'>"
        "</body>"
        "</html>";

    return httpd_resp_send(
        request,
        webpage,
        HTTPD_RESP_USE_STRLEN
    );
}

// =========================================================
// START HTTP SERVER
// =========================================================

void startCameraServer()
{
    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    config.server_port =
        80;

    config.max_open_sockets =
        4;

    httpd_uri_t rootUri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = indexHandler,
        .user_ctx = NULL
    };

    httpd_uri_t streamUri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = streamHandler,
        .user_ctx = NULL
    };

    if(
        httpd_start(
            &cameraServer,
            &config
        ) == ESP_OK
    )
    {
        httpd_register_uri_handler(
            cameraServer,
            &rootUri
        );

        httpd_register_uri_handler(
            cameraServer,
            &streamUri
        );
    }
}

// =========================================================
// CAMERA INITIALIZATION
// =========================================================

bool initCamera()
{
    camera_config_t config;

    config.ledc_channel =
        LEDC_CHANNEL_0;

    config.ledc_timer =
        LEDC_TIMER_0;

    config.pin_d0 =
        Y2_GPIO_NUM;

    config.pin_d1 =
        Y3_GPIO_NUM;

    config.pin_d2 =
        Y4_GPIO_NUM;

    config.pin_d3 =
        Y5_GPIO_NUM;

    config.pin_d4 =
        Y6_GPIO_NUM;

    config.pin_d5 =
        Y7_GPIO_NUM;

    config.pin_d6 =
        Y8_GPIO_NUM;

    config.pin_d7 =
        Y9_GPIO_NUM;

    config.pin_xclk =
        XCLK_GPIO_NUM;

    config.pin_pclk =
        PCLK_GPIO_NUM;

    config.pin_vsync =
        VSYNC_GPIO_NUM;

    config.pin_href =
        HREF_GPIO_NUM;

    config.pin_sccb_sda =
        SIOD_GPIO_NUM;

    config.pin_sccb_scl =
        SIOC_GPIO_NUM;

    config.pin_pwdn =
        PWDN_GPIO_NUM;

    config.pin_reset =
        RESET_GPIO_NUM;

    // =====================================================
    // IMPORTANT RHYX SETTINGS
    // =====================================================

    config.xclk_freq_hz =
        20000000;

    config.pixel_format =
        PIXFORMAT_RGB565;

    config.frame_size =
        CAMERA_FRAME_SIZE;

    config.grab_mode =
        CAMERA_GRAB_WHEN_EMPTY;

    // For raw RGB on classic ESP32, use ONE framebuffer.
    //
    // Espressif recommends multiple buffers primarily
    // with JPEG.

    config.fb_count =
        1;

    // jpeg_quality doesn't control RGB565,
    // because the sensor isn't generating JPEG.

    config.jpeg_quality =
        12;

    if(psramFound())
    {
        Serial.println(
            "PSRAM detected."
        );

        config.fb_location =
            CAMERA_FB_IN_PSRAM;
    }
    else
    {
        Serial.println(
            "WARNING: PSRAM not detected."
        );

        config.fb_location =
            CAMERA_FB_IN_DRAM;

        // Without PSRAM you should use QQVGA.
        config.frame_size =
            FRAMESIZE_QQVGA;
    }

    esp_err_t error =
        esp_camera_init(
            &config
        );

    if(error != ESP_OK)
    {
        Serial.printf(
            "Camera initialization failed: 0x%x\n",
            error
        );

        return false;
    }

    // Print detected camera information.

    sensor_t *sensor =
        esp_camera_sensor_get();

    if(sensor)
    {
        Serial.printf(
            "Camera PID: 0x%02X\n",
            sensor->id.PID
        );

        Serial.println(
            "Camera detected successfully."
        );
    }

    return true;
}

// =========================================================
// WIFI
// =========================================================

void connectWiFi()
{
    WiFi.mode(
        WIFI_STA
    );

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    // Major streaming improvement:
    // prevent WiFi modem sleep.

    WiFi.setSleep(
        false
    );

    Serial.print(
        "Connecting WiFi"
    );

    while(
        WiFi.status() !=
        WL_CONNECTED
    )
    {
        Serial.print(".");
        delay(400);
    }

    Serial.println();

    Serial.println(
        "WiFi connected."
    );

    Serial.print(
        "Camera IP: "
    );

    Serial.println(
        WiFi.localIP()
    );

    Serial.print(
        "Stream: http://"
    );

    Serial.print(
        WiFi.localIP()
    );

    Serial.println(
        "/stream"
    );
}

// =========================================================
// SETUP
// =========================================================

void setup()
{
    Serial.begin(
        115200
    );

    Serial.println();
    Serial.println(
        "RHYX M21-45 ESP32-CAM"
    );

    if(!initCamera())
    {
        Serial.println(
            "Camera failed."
        );

        while(true)
        {
            delay(1000);
        }
    }

    connectWiFi();

    startCameraServer();

    Serial.println(
        "Camera web server started."
    );
}

// =========================================================
// LOOP
// =========================================================

void loop()
{
    delay(10000);
}