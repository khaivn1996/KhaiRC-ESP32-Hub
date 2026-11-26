#pragma once

// Optional local-only file: copy to include/tuya_config.h, then fill in locally.
// NEVER commit or paste the actual local_key into GitHub/chat/build logs.
// Rang Dong OC09 "O cam 4" over Tuya LAN protocol 3.4 (TCP 6668).
//
// How to get the values (on a PC in the same LAN):
//   pip install tinytuya
//   python -m tinytuya wizard   -> Tuya IoT Access ID/Secret, region Singapore/EU
//                                  devices.json: "id", "key", mapping switch_4 -> DP id
//   python -m tinytuya scan     -> IP address and Ver (must be 3.4)
// Reserve a DHCP lease for the plug in your router so the IP does not change.
#define TUYA_OC09_IP "192.168.x.x"
#define TUYA_OC09_DEVICE_ID "REPLACE_WITH_TUYA_DEVICE_ID"
// The LOCAL key is exactly 16 characters (not the cloud Access Secret).
#define TUYA_OC09_LOCAL_KEY "REPLACE_WITH_16_CHAR_LOCAL_KEY"
// DP id of switch_4 ("O cam 4").
#define TUYA_OC09_DP 4
