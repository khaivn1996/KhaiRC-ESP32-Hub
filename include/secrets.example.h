#pragma once

// Copy file nay thanh "include/secrets.h" va dien thong tin thuc te.
// KHONG commit "include/secrets.h" len Git.

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// HUB_API_KEY bat buoc phai dung 64 ky tu (sizeof == 65 tinh ca ky tu ket thuc '\0')
// de thoa man static_assert trong src/CommandApi.cpp.
// Co the tao nhanh bang PowerShell:
// -join ((1..64) | ForEach-Object { '{0:x}' -f (Get-Random -Max 16) })
#define HUB_API_KEY "0000000000000000000000000000000000000000000000000000000000000000"