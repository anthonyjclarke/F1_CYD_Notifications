# Telegram Notification Setup Guide

This guide explains how to configure Telegram notifications for this project (`F1 CYD Notifications`) using the `Universal-Arduino-Telegram-Bot` library.

## 1. What This Project Uses Telegram For

In this firmware, Telegram is used to **send outbound notifications**:
- Race week notification
- Pre-session notifications (1 hour before)
- Race result notification
- Manual verification message from the Web UI
- Manual resend of the last successfully sent Telegram message

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

When Telegram credentials are changed in the Web UI, the firmware immediately re-initializes the bot and sends a confirmation message to the configured chat. The save banner reports whether that confirmation was sent.

The Config tab also includes:
- `Test Telegram` - sends a fresh verification message using the saved token/chat ID.
- `Resend Last` - resends the last Telegram message that was successfully sent and saved by the device.

### Using `Test Telegram`

1. Open `http://<device-ip>/`.
2. Select the Config tab.
3. Confirm Bot Token and Chat ID are populated.
4. If either value changed, click `Save Configuration` first.
5. Click `Test Telegram`.

Expected Telegram message:

```text
F1 Display Telegram test

Telegram credentials are configured and this chat can receive messages.
```

If the Web UI reports `Telegram test failed`, check the troubleshooting section below.

### Using `Resend Last`

Click `Resend Last` from the Config tab to resend the last Telegram message that the ESP32 successfully delivered. This message is saved to LittleFS at `/telegram_last.txt`, so it survives reboot.

`Resend Last` is useful after replacing credentials or confirming the bot still works, but it cannot recreate an event notification that never sent successfully.

Stored in `/config.json` as:
- `bot`
- `chat`
- `tgOn`

## 6. Verify Notifications Are Enabled

From firmware logic:
- `telegramEnabled` is set true when both bot token and chat ID are non-empty.
- The Web UI confirmation or `Test Telegram` button verifies that Telegram accepted the send.

So both must be configured:
- Token
- Chat ID

Related local API endpoints:
- `GET /api/telegram/status`
- `POST /api/telegram/test`
- `POST /api/telegram/resend`

## 7. Notification Timing in This Project

Implemented notifications:
- Race week (first notification check within the race week window)
- Pre-session (1 hour before):
  - Qualifying
  - Sprint Qualifying
  - Sprint
  - Race
- Results after post-race data is available

Deduplication:
- Per-round bitmask persisted in config to avoid duplicate sends.

## 8. Known Behavior / Current Limitation

The device stores the last successfully sent Telegram message in LittleFS so `Resend Last` can survive a reboot.

Limits:
- If no Telegram message has ever been sent successfully, there is nothing to resend.
- `Resend Last` does not reconstruct a missed event notification; it only resends the saved last-successful message.

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

- Use `Test Telegram` in the Web UI.
- If the test fails, check token/chat ID and confirm you have sent `/start` to the bot.
- If the test succeeds but an event notification was missed, use `Resend Last` only if the previous message had already been sent successfully.

## 11. Security Notes

- Treat bot token as a secret.
- Do not commit real bot tokens/chat IDs to git.
- Rotate token via BotFather if exposed.

## 12. References

- Library README: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
- Telegram Bot API: https://core.telegram.org/bots/api
- BotFather: https://t.me/BotFather
