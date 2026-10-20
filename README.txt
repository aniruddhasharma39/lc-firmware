LC Gate Master firmware — direct certificate MQTT test

Files: LCGateMasterFirmware.ino

What changed
- Embedded the uploaded AWS IoT device certificate and matching private key. OpenSSL public-key checks confirmed that the certificate and private key match.
- First boot now falls back to these embedded device credentials rather than stopping at empty Fleet Provisioning claim placeholders.
- Uses Thing/client ID LC-M-001, AWS endpoint a18ey7y7yi6gdo-ats.iot.ap-south-1.amazonaws.com, and publishes to lc-gate/LC-M-001/telemetry and /status.

Important limitations
- The uploaded certificate is an AWS IoT device certificate, not proven to be a Fleet Provisioning claim certificate. This build therefore uses it directly; it does not create/register a new Thing through Fleet Provisioning.
- I cannot verify from the PEM files alone whether the certificate is ACTIVE in AWS IoT, which policy is attached, or whether the policy allows client ID LC-M-001 and publish actions on both topics. If AWS rejects CONNECT/PUBLISH, check certificate status and policy in AWS IoT Core.
- This source has not been compiled against your exact board/core or tested on the physical ESP32/A7672S modem. Existing MQTT-EX command syntax and modem URC parsing remain hardware-validation items.
- The uploaded private key is now embedded in this source and was shared in this conversation. Treat it as exposed and rotate/deactivate it after testing. Never commit this sketch to a public repository.

Hardware settings in sketch
- Modem UART2: ESP32 RX GPIO18, TX GPIO17, 115200 baud
- Limit switch GPIO23 HIGH = closed
- Hall sensor GPIO22 LOW = closed
- APN airtelgprs.com
