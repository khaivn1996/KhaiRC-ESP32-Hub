#pragma once

// Copy this file to include/web_password.h (ignored by Git).
// Set a Web UI password of 1–64 bytes, counted in UTF-8. Use a separate value from HUB_API_KEY.
// ASCII control bytes (including tab, newline, embedded NUL and DEL) are rejected at compile time.
// An empty or missing password disables password login. Web Vault still uses its API key.
#define HUB_WEB_PASSWORD ""
