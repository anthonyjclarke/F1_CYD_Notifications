#pragma once

// POSIX rules for the curated IANA zones offered by the web UI (web_server.h).
// Applied locally with ezTime setPosix(), so the timezone never depends on the
// network. ezTime's online lookup (timezoned.rop.nl) times out often and serves
// stale rules (Mexico_City still with DST, Cairo without). Rules from tzdata
// 2025; "GMT+N" / "GMT-N" are handled in time_utils.h as UTC+N / UTC-N.
struct TimezonePosix {
    const char* name;
    const char* posix;
};

static constexpr TimezonePosix TIMEZONE_POSIX[] = {
    {"UTC",                 "UTC0"},
    // Europe
    {"Europe/London",       "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Paris",        "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Amsterdam",    "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Berlin",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Rome",         "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Madrid",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Zurich",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Vienna",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Brussels",     "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Prague",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Warsaw",       "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Moscow",       "MSK-3"},
    {"Europe/Istanbul",     "<+03>-3"},
    // Americas
    {"America/New_York",    "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Toronto",     "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago",     "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver",      "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Anchorage",   "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Mexico_City", "CST6"},
    {"America/Bogota",      "<-05>5"},
    {"America/Buenos_Aires","<-03>3"},
    {"America/Sao_Paulo",   "<-03>3"},
    // Asia
    {"Asia/Dubai",          "<+04>-4"},
    {"Asia/Bangkok",        "<+07>-7"},
    {"Asia/Hong_Kong",      "HKT-8"},
    {"Asia/Shanghai",       "CST-8"},
    {"Asia/Tokyo",          "JST-9"},
    {"Asia/Seoul",          "KST-9"},
    {"Asia/Singapore",      "<+08>-8"},
    {"Asia/Kolkata",        "IST-5:30"},
    // Australia
    {"Australia/Sydney",    "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Australia/Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Australia/Brisbane",  "AEST-10"},
    {"Australia/Adelaide",  "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"Australia/Perth",     "AWST-8"},
    // Africa
    {"Africa/Johannesburg", "SAST-2"},
    {"Africa/Cairo",        "EET-2EEST,M4.5.5/0,M10.5.4/24"},
    {"Africa/Lagos",        "WAT-1"},
};

static constexpr size_t TIMEZONE_POSIX_COUNT = sizeof(TIMEZONE_POSIX) / sizeof(TIMEZONE_POSIX[0]);

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
