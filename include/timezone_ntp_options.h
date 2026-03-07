#pragma once

// Curated list of popular IANA timezones and GMT offsets for WebUI dropdowns

// IANA timezone strings (for real-world locations)
static constexpr const char* TIMEZONE_OPTIONS[] = {
    // UTC
    "UTC",
    
    // Europe
    "Europe/London",
    "Europe/Paris",
    "Europe/Amsterdam",
    "Europe/Berlin",
    "Europe/Rome",
    "Europe/Madrid",
    "Europe/Zurich",
    "Europe/Vienna",
    "Europe/Brussels",
    "Europe/Prague",
    "Europe/Warsaw",
    "Europe/Moscow",
    "Europe/Istanbul",
    
    // Americas
    "America/New_York",
    "America/Toronto",
    "America/Chicago",
    "America/Denver",
    "America/Los_Angeles",
    "America/Anchorage",
    "America/Mexico_City",
    "America/Toronto",
    "America/Bogota",
    "America/Buenos_Aires",
    "America/Sao_Paulo",
    
    // Asia
    "Asia/Dubai",
    "Asia/Bangkok",
    "Asia/Hong_Kong",
    "Asia/Shanghai",
    "Asia/Tokyo",
    "Asia/Seoul",
    "Asia/Singapore",
    "Asia/Kolkata",
    "Asia/Bangkok",
    
    // Australia
    "Australia/Sydney",
    "Australia/Melbourne",
    "Australia/Brisbane",
    "Australia/Adelaide",
    "Australia/Perth",
    
    // Africa
    "Africa/Johannesburg",
    "Africa/Cairo",
    "Africa/Lagos",
    
    // GMT Offsets (for flexibility)
    "GMT-12",
    "GMT-11",
    "GMT-10",
    "GMT-9",
    "GMT-8",
    "GMT-7",
    "GMT-6",
    "GMT-5",
    "GMT-4",
    "GMT-3",
    "GMT-2",
    "GMT-1",
    "GMT+0",
    "GMT+1",
    "GMT+2",
    "GMT+3",
    "GMT+4",
    "GMT+5",
    "GMT+6",
    "GMT+7",
    "GMT+8",
    "GMT+9",
    "GMT+10",
    "GMT+11",
    "GMT+12",
    "GMT+13",
    "GMT+14",
};

static constexpr size_t TIMEZONE_OPTIONS_COUNT = sizeof(TIMEZONE_OPTIONS) / sizeof(TIMEZONE_OPTIONS[0]);

// NTP Server URLs (commonly used public NTP servers)
static constexpr const char* NTP_SERVER_OPTIONS[] = {
    "pool.ntp.org",           // NTP Pool (global, recommended)
    "time.nist.gov",          // NIST (USA)
    "time.google.com",        // Google Public NTP
    "time.cloudflare.com",    // Cloudflare
    "0.pool.ntp.org",         // NTP Pool (tier-0)
    "1.pool.ntp.org",         // NTP Pool (tier-1)
    "2.pool.ntp.org",         // NTP Pool (tier-2)
    "3.pool.ntp.org",         // NTP Pool (tier-3)
    "time.apple.com",         // Apple (global)
    "time.windows.com",       // Windows (Microsoft)
    "ntp.ubuntu.com",         // Ubuntu
    "0.amazon.pool.ntp.org",  // Amazon NTP Pool
    "time1.google.com",       // Google (backup)
    "time2.google.com",       // Google (backup)
};

static constexpr size_t NTP_SERVER_OPTIONS_COUNT = sizeof(NTP_SERVER_OPTIONS) / sizeof(NTP_SERVER_OPTIONS[0]);
