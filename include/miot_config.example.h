#pragma once

// Optional local-only file: copy to include/miot_config.h, then fill in locally.
// NEVER commit or paste the actual LAN token into GitHub/chat/build logs.
// The IP may change; reserve a DHCP lease for this device in your router.
#define MILIFE_IP "192.168.x.x"
//did lay tu lenh: mi.exe lan discover
#define MILIFE_DID xxxxxxxxxxxx
// The device's LOCAL miIO token is 32 hex chars (16 bytes), not a Xiaomi OAuth token.
#define MILIFE_TOKEN "REPLACE_WITH_32_HEX_CHAR_LOCAL_TOKEN"
