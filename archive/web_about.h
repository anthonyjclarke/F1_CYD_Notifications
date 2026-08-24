// =============================================================
// web_about.h — Reusable "About" section for ESPAsyncWebServer projects
//
// Usage:
//   1. Paste WEB_ABOUT_CSS into your page's <style> block.
//   2. Paste WEB_ABOUT_HTML where you want the section to appear
//      (e.g. bottom of config tab, inside a <div id="tab-cfg">).
//   3. Customise the #define values below for each project.
//
// Customise per-project:
//   ABOUT_PROJECT_NAME   — display name
//   ABOUT_GITHUB_URL     — full GitHub repo URL
//   ABOUT_BLUESKY_URL    — full Bluesky profile URL
//   ABOUT_BLUESKY_HANDLE — display handle (no @)
//   ABOUT_CREDIT_NAME    — credit name/handle shown as link text
//   ABOUT_CREDIT_URL     — credit URL
//   ABOUT_AUTHOR         — your name
// =============================================================

#pragma once

// --- Customise these per project ---
#define ABOUT_PROJECT_NAME   "F1 CYD Notifications"
#define ABOUT_GITHUB_URL     "https://github.com/anthonyjclarke/F1_CYD_Notifications"
#define ABOUT_BLUESKY_URL    "https://bsky.app/profile/anthonyjclarke.bsky.social"
#define ABOUT_BLUESKY_HANDLE "anthonyjclarke.bsky.social"
#define ABOUT_CREDIT_NAME    "@witnessmenow"
#define ABOUT_CREDIT_URL     "https://github.com/witnessmenow/F1-Arduino-Notifications"
#define ABOUT_AUTHOR         "Anthony Clarke"

// --- CSS snippet: paste inside your page <style> block ---
//
// .about{text-align:center;margin-top:20px;padding-top:14px;border-top:1px solid #2a2a3e;font-size:0.8em}
// .about-links{margin-bottom:6px}
// .about-links a{color:#4caf50;text-decoration:none;font-weight:bold;font-size:1.05em}
// .about-links a:hover{text-decoration:underline}
// .about-built{color:#aaa;margin-bottom:4px}
// .about-credit{color:#555}
// .about-credit a{color:#555}

// --- HTML snippet: paste where about section should appear ---
//
// <div class="about">
// <div class="about-links">
// <a href="ABOUT_GITHUB_URL" target="_blank">GitHub</a>
// &nbsp;|&nbsp;
// <a href="ABOUT_BLUESKY_URL" target="_blank">Bluesky</a>
// </div>
// <div class="about-built">Built with &#10084; by ABOUT_AUTHOR</div>
// <div class="about-credit">Based on original idea by
//   <a href="ABOUT_CREDIT_URL" target="_blank">ABOUT_CREDIT_NAME</a>
// </div>
// </div>
