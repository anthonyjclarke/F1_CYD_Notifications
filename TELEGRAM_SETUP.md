# Telegram Notification Setup Guide

This guide explains how to configure Telegram notifications for this project (`F1 CYD Notifications`) using the `Universal-Arduino-Telegram-Bot` library.

## 1. What This Project Uses Telegram For

In this firmware, Telegram is used to **send outbound notifications**:
- Race week notification
- Pre-session notifications (1 hour before)
- Race result notification

This project does **not** currently read inbound Telegram commands/messages.

## 2. Prerequisites

- A Telegram account
- Device connected to Wi-Fi
- Valid time sync (NTP) recommended
- Telegram bot token and chat ID

Library used:
- `Universal-Arduino-Telegram-Bot`
- Repo: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot

## 3. Create a Telegram Bot (BotFather)

1. Open Telegram and search for `@BotFather`.
2. Start chat and send `/newbot`.
3. Follow prompts for:
- Bot display name
- Bot username (must end with `bot`)
4. BotFather returns a token like:
- `123456789:AA...`
5. Save this token securely.

## 4. Get Your Chat ID

You need the numeric chat ID where notifications will be sent.

### Option A (simple): Use an ID bot

1. Message `@userinfobot` or `@myidbot`.
2. Read the returned numeric ID.

### Option B (direct API): Use `getUpdates`

1. Open chat with your new bot and send `/start` (or any message).
2. Visit:
   - `https://api.telegram.org/bot<YOUR_BOT_TOKEN>/getUpdates`
3. Find `chat.id` in JSON response.

Important:
- Bots can only message users/chats that have interacted with the bot first.

## 5. Configure This Firmware

You can configure Telegram fields in two ways.

### Method 1: Captive Portal (first-time Wi-Fi setup)

WiFiManager includes:
- Telegram Bot Token
- Telegram Chat ID

### Method 2: Web UI

Open:
- `http://<device-ip>/`

Set:
- `Bot Token`
- `Chat ID`

Then save.

Stored in `/config.json` as:
- `bot`
- `chat`
- `tgOn`

## 6. Verify Notifications Are Enabled

From firmware logic:
- `telegramEnabled` is set true when bot token is non-empty.
- Sending still requires a valid non-empty chat ID.

So both must be configured:
- Token
- Chat ID

## 7. Notification Timing in This Project

Implemented notifications:
- Race week (Monday within race week window)
- Pre-session (1 hour before):
  - Qualifying
  - Sprint Qualifying
  - Sprint
  - Race
- Results after post-race data is available

Deduplication:
- Per-round bitmask persisted in config to avoid duplicate sends.

## 8. Known Behavior / Current Limitation

If Telegram token is changed from the Web UI, the running bot instance is not re-initialized immediately in current code path.

Recommendation:
- Reboot after changing token/chat ID to guarantee new credentials are active.

## 9. Group Chat Setup (Optional)

To notify a group:
1. Add your bot to the group.
2. Send a message in that group.
3. Use `getUpdates` to find the group `chat.id` (often negative).
4. Put that group ID into `Chat ID`.

If bot does not receive expected group updates, check BotFather privacy mode settings for your use case.

## 10. Troubleshooting

### No messages received

Check:
- Device has internet access
- Bot token is valid
- Chat ID is correct
- You have sent `/start` (or any message) to bot/chat
- Telegram is enabled in config

### Logs show send failed

Check serial logs for `[Telegram]` lines and verify:
- Chat ID is non-empty
- Token is correct
- Network is stable

### Notifications not appearing after config change

- Reboot device after changing Telegram fields.

## 11. Security Notes

- Treat bot token as a secret.
- Do not commit real bot tokens/chat IDs to git.
- Rotate token via BotFather if exposed.

## 12. References

- Library README: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
- Telegram Bot API: https://core.telegram.org/bots/api
- BotFather: https://t.me/BotFather
