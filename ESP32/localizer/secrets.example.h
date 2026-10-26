#pragma once

// Copy this file to secrets.h (git-ignored) and fill it in.
// The network must be 2.4 GHz: the ESP32 cannot join 5 GHz networks.

#define WIFI_SSID     "your-2.4GHz-network"
#define WIFI_PASSWORD "your-password"

// Machine running localization_api (uvicorn on port 8000).
// Windows Mobile Hotspot always puts the laptop at 192.168.137.1. On a phone
// hotspot this is only a first guess: the board remembers the last address
// that worked and probes the subnet for /health when that fails.
#define API_BASE_URL  "http://192.168.137.1:8000"
