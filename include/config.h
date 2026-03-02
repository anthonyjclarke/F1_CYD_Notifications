#pragma once

// =============================================================
// F1 CYD Notifications - Configuration
// ESP32-2432S028R (2.8" Cheap Yellow Display)
// =============================================================

// --- App Version ---
#define APP_VERSION         "0.3.0"

// --- CYD Pin Mapping (v1/v2) ---
// TFT display pins defined in platformio.ini build_flags
#define PIN_BL          21    // Backlight (PWM)
#define PIN_TOUCH_CS    33
#define PIN_TOUCH_IRQ   36
#define PIN_LED_R        4    // RGB LED
#define PIN_LED_G       16
#define PIN_LED_B       17
#define PIN_LDR         34    // Light dependent resistor
#define PIN_SD_CS        5    // MicroSD CS (CYD expansion / onboard SD slot)
#define PIN_SD_MOSI     23    // MicroSD MOSI
#define PIN_SD_MISO     19    // MicroSD MISO
#define PIN_SD_SCK      18    // MicroSD SCK
#define PIN_SHOT_BTN    27    // Optional screenshot button (active LOW)

// --- Display ---
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240
#define TFT_ROTATION     1    // Landscape

// --- Backlight PWM ---
#define BL_PWM_CHANNEL   0
#define BL_PWM_FREQ   5000
#define BL_PWM_RES       8    // 8-bit (0-255)

// --- WiFi ---
#define WIFI_AP_NAME        "F1-Display"
#define WIFI_AP_PASSWORD    ""
#define WIFI_TIMEOUT_SEC    180

// --- File Paths (LittleFS) ---
#define CONFIG_FILE         "/config.json"
#define RACE_CACHE_FILE     "/races.json"
#define RESULTS_CACHE_FILE  "/results.json"

// --- F1 Data URLs ---
#define SCHEDULE_URL        "https://raw.githubusercontent.com/sportstimes/f1/main/_db/f1/2026.json"
#define JOLPICA_BASE_URL    "http://api.jolpi.ca/ergast/f1/2026"

// --- Timing Constants ---
#define SCHEDULE_REFRESH_MS     (24UL * 60 * 60 * 1000)  // 24 hours
#define RESULTS_RETRY_MS        (30UL * 60 * 1000)        // 30 minutes
#define RESULTS_POLL_AFTER_SEC  (3 * 60 * 60)             // 3 hours after GP
#define RESULTS_GIVE_UP_SEC     (24 * 60 * 60)            // Stop retrying after 24h
#define DISPLAY_ROTATE_MS       8000                       // Screen rotation
#define POST_RACE_ROTATE_MS     10000                      // Post-race rotation
#define COUNTDOWN_UPDATE_MS     1000                       // 1 second
#define NOTIFICATION_CHECK_MS   (60UL * 1000)              // 1 minute
#define BRIGHTNESS_CHECK_MS     (10UL * 1000)              // 10 seconds

// --- Race Week ---
#define COUNTDOWN_WEEK_DAYS     7     // Days before race to start countdown
#define POST_RACE_DAYS          7     // Days to show post-race data
#define NOTIFY_HOURS_BEFORE     1     // Hours before session to notify

// --- Display Colors (RGB565) ---
#define COLOR_BG            TFT_BLACK
#define COLOR_F1_RED        0xF800
#define COLOR_TEXT           TFT_WHITE
#define COLOR_GRID           0x4208    // Dark gray
#define COLOR_HIGHLIGHT      0xFFE0    // Yellow
#define COLOR_COUNTDOWN      TFT_WHITE
#define COLOR_TRACK          0xF800    // Red
#define COLOR_HEADER_BG      0xF800    // F1 Red
#define COLOR_HEADER_TEXT     TFT_WHITE
#define COLOR_SESSION_TEXT    0xCE59    // Light gray
#define COLOR_PODIUM_GOLD    0xFEA0
#define COLOR_PODIUM_SILVER  0xC618
#define COLOR_PODIUM_BRONZE  0xCC60

// --- Limits ---
#define MAX_RACES            3     // Store prev/current/next
#define MAX_SESSIONS         7     // Max sessions per race
#define MAX_STANDINGS        10    // Top 10 standings
#define MAX_PODIUM           3     // Podium positions

// --- Web Server ---
#define WEB_SERVER_PORT      80

// --- Screenshot Capture ---
#define SCREENSHOT_WEB_ENABLED     1
#define SCREENSHOT_BUTTON_ENABLED  0
#define SCREENSHOT_DEBOUNCE_MS     250
#define SCREENSHOT_DIR             "/shots"
#define SCREENSHOT_STARTUP_CAPTURES 0   // Set to 0 before release builds
