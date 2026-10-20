/*
  LC Gate Master - ESP32 + SIMCom A7672S-LASC
  AWS IoT Core Cloud Connectivity Firmware (Fleet Provisioning & Direct Certificate)

  Hardware Pinout:
    ESP32 UART2 RX = GPIO18  <--- Connected to Modem TXD
    ESP32 UART2 TX = GPIO17  ---> Connected to Modem RXD
    Baud rate: 115200 baud
    GPIO23: Limit switch (LOW = CLOSED, internal INPUT_PULLUP)
    GPIO22: Hall sensor (LOW = CLOSED, internal INPUT_PULLUP)
    GND: ESP32 GND directly connected to A7672S GND (Common Ground)

  Critical Hardware & RF Requirements:
    1. A7672S power: The module requires 3.4V - 4.2V (or 5V if dev board has onboard LDO/buck).
       It draws up to 2A peak current during RF bursts and TLS handshakes.
       DO NOT power the modem from the ESP32 3.3V pin. Use a dedicated 5V 2A-3A DC power source.
    2. LTE Main Antenna must be securely screwed onto the MAIN antenna port.
*/

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>

HardwareSerial Modem(2);
Preferences prefs;

static const int GSM_RX_PIN = 18; // ESP32 RX <- Modem TX
static const int GSM_TX_PIN = 17; // ESP32 TX -> Modem RX
static const int LIMIT_PIN  = 23;
static const int HALL_PIN   = 22;

static const char AWS_HOST[]      = "a18ey7y7yi6gdo-ats.iot.ap-south-1.amazonaws.com";
static const char TEMPLATE_NAME[] = "LCGateMasterProvisioningTemplate";
static const char FACTORY_ID[]    = "LC-M-001";
static const char APN[]           = "airtelgprs.com";

static const char ROOT_CA_FILE[]     = "rootca.pem";
static const char CLAIM_CERT_FILE[]  = "claimcert.pem";
static const char CLAIM_KEY_FILE[]   = "claimkey.pem";
static const char DEVICE_CERT_FILE[] = "devicecert.pem";
static const char DEVICE_KEY_FILE[]  = "devicekey.pem";

// -----------------------------------------------------------------------------
// Claim Certificate & Key for Fleet Provisioning
// -----------------------------------------------------------------------------
static const char CLAIM_CERT[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDWTCCAkGgAwIBAgIUZClb11PGaZjQfr+fKpE4kwzIWfAwDQYJKoZIhvcNAQEL
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI2MTAxNzExMzkz
OVoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALAssnb+9g9A56+zWZTa
xLQQSLwHhaoMQTxnvdQTOafsL2QpGChsCXK7TdWehsy9w6cbZeWgvGfq4ubdWeuI
NYLu+asJzitleMq1Z4k4kaR5XRpo67OZo7hKwUgKyZVD5MtqJNTU/HKUEDnk4LAR
DDH9ldpWacDHLwlpRUw37SQiE0mLbEsl4HhYGS22tczgdZ31HMT7ORUuBZqpnEiS
rt3RnNWCJbiwXjEE4k1JqD8xeSG8Gye8EpJreHNhQIgObQz78NVgc9zleg6NTE77
IYuNoTk+EbzJ3nHCCUH8pgwwfPwWnJcipuEP+H1qHO3oooFCbKHdCU2ygLofRyka
IW0CAwEAAaNgMF4wHwYDVR0jBBgwFoAUj7n7CADh4ykz1BHuakC29zhn080wHQYD
VR0OBBYEFDEd6FP/KHX5XCWyfuaQ7ITASIxMMAwGA1UdEwEB/wQCMAAwDgYDVR0P
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQA/1fViddmk0+t9CG6lMSPk/8Xl
6qIUhK1g52wSEHjJsaQy5yh6RIF4NdW9D5c8sw5cSyxUecJShrDVM3yMvtvRc6rL
2cjwyjWTDTuiuZVmcSkNgf/DDp3JzE0FdBBrTHsWnEF8PJAwYs+bh/ZOWXrBE+Wb
WgeS29hsZTliGallxCKMx1iy5zIasBMValb3FWq6dPMew42Xid8TiZoa9ll/+NLo
BvPaGv1uEdPcbDS+HHzGCDeLunz0JScYnU4fJ3LnMhx4Go2SBywfp22BAQiogA4P
MKGulA4KDsOqhstv3bgFhweBlAgV3BDoYMZUFAYbp3izdkhxY6xqBvQdH8h4
-----END CERTIFICATE-----
)PEM";

static const char CLAIM_KEY[] PROGMEM = R"PEM(
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAsCyydv72D0Dnr7NZlNrEtBBIvAeFqgxBPGe91BM5p+wvZCkY
KGwJcrtN1Z6GzL3Dpxtl5aC8Z+ri5t1Z64g1gu75qwnOK2V4yrVniTiRpHldGmjr
s5mjuErBSArJlUPky2ok1NT8cpQQOeTgsBEMMf2V2lZpwMcvCWlFTDftJCITSYts
SyXgeFgZLba1zOB1nfUcxPs5FS4FmqmcSJKu3dGc1YIluLBeMQTiTUmoPzF5Ibwb
J7wSkmt4c2FAiA5tDPvw1WBz3OV6Do1MTvshi42hOT4RvMneccIJQfymDDB8/Bac
lyKm4Q/4fWoc7eiigUJsod0JTbKAuh9HKRohbQIDAQABAoIBAAME0Q4O8mJyh0XQ
efF+Ci7XtJdcPaMbUKbbVDplFfDVsD40IhDXQRYxNXU6oS6y9b5QTt+vcGsLcFXq
ZxcNH3Wd6LNQmLLPq1E16I9GY+CAn+LK93b9Y/vNdCBq5udi4XHG4NiwyZ6hAOIt
FNiPgjj9EmtKXceK4jh0EFbtAmhDEe17zGMbHXJpSdpOqYf8PeyL4CA+Kg9bCUt9
pg3EjQYrpqFcz+lhvn+YUBEX1WXmwwV6LOLJtkCSXwpFrcWzU8QuwQRFTTEccXjS
yj5C57+UdyVnxFb6lCE+uXU0mzbU3VmHf4qMuWRHlp3nOUbgMMy1HiOFQp4hvNdA
9fswJzUCgYEA4G+dms4c8GCqWiYNRsZ9GcUyBpoPh/LL7hqMJ4ubIFRcBRGjqF6k
sBixeSbiVLX5S5iVzkN+dF2qtHDIcdlDvghsoMiAigFagwtXr0RkfCNtjoKVMCQg
lbDfduq5fnbtSw9zTmqGjeFAETa6E6DpALNY6ZupOYMz/TsAuViNDxsCgYEAyPOF
5cx6kHT5mNMdC/E9T1tt52ihw3sJuQEnXLUUGW244v024D4VvNXN5x6s+Gl/F155
4RMQePvZPvZZ4UoCp9W1kI69Ys+LSbOXZLksL6sY1z7FqZNv0DPPfZzGl6PVgSo4
1ONfs45KzEPCsCU6EGN5KYShM4xoaF8FpxWXshcCgYEAoGBB3/AvD7oToQ2V6SBH
HtYh1EPcm0ntXbzMsCl7dVOy2dBsARF4X3mVwDv/+YZ7mQB09AqY3zBbnksFgQ0+
8hl0/kOoTnV/JytAwjZ+E5MBY5d+YgM+4ZidZo1+dZcoI73rqHpIojP2oxZXS1Vt
ipmeDHi2KSnPScIS118mlUcCgYEAvhy2BqwnIFeqc4z3I/89E8DZ/14b/EaA8khH
JwwhUdVeX1FUA155TkewHwIEfpfAJym4gNl6chtCcrC6Y8uTK5mhxntkbQERM872
YdJWHVrvi2PVuRLFa+I4YQxbTccwDJ0XW0Epn0P1O763h9IQVsTlMgRKReyGMogD
XwmslE0CgYBfe/JAu2j5HuQKnQyP0IsJogz3yv7JEk408Xa1bm7sNreVAWi1rz2L
aesnedhaEUyAyKg6ILKw0+cIFcH4HXL5I5QTvOOFoDiaGZdJc9ie3XDvhwpJOJJq
YGtXg3qQrbam0faJ1D46Lv1ou1QWXhTNsTChSGgfu8OUVx8iTNd1AA==
-----END RSA PRIVATE KEY-----
)PEM";

// -----------------------------------------------------------------------------
// Direct Certificate Fallback (Pre-provisioned device certificate)
// -----------------------------------------------------------------------------
static const char BOOT_DEVICE_CERT[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDWTCCAkGgAwIBAgIUZClb11PGaZjQfr+fKpE4kwzIWfAwDQYJKoZIhvcNAQEL
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI2MTAxNzExMzkz
OVoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALAssnb+9g9A56+zWZTa
xLQQSLwHhaoMQTxnvdQTOafsL2QpGChsCXK7TdWehsy9w6cbZeWgvGfq4ubdWeuI
NYLu+asJzitleMq1Z4k4kaR5XRpo67OZo7hKwUgKyZVD5MtqJNTU/HKUEDnk4LAR
DDH9ldpWacDHLwlpRUw37SQiE0mLbEsl4HhYGS22tczgdZ31HMT7ORUuBZqpnEiS
rt3RnNWCJbiwXjEE4k1JqD8xeSG8Gye8EpJreHNhQIgObQz78NVgc9zleg6NTE77
IYuNoTk+EbzJ3nHCCUH8pgwwfPwWnJcipuEP+H1qHO3oooFCbKHdCU2ygLofRyka
IW0CAwEAAaNgMF4wHwYDVR0jBBgwFoAUj7n7CADh4ykz1BHuakC29zhn080wHQYD
VR0OBBYEFDEd6FP/KHX5XCWyfuaQ7ITASIxMMAwGA1UdEwEB/wQCMAAwDgYDVR0P
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQA/1fViddmk0+t9CG6lMSPk/8Xl
6qIUhK1g52wSEHjJsaQy5yh6RIF4NdW9D5c8sw5cSyxUecJShrDVM3yMvtvRc6rL
2cjwyjWTDTuiuZVmcSkNgf/DDp3JzE0FdBBrTHsWnEF8PJAwYs+bh/ZOWXrBE+Wb
WgeS29hsZTliGallxCKMx1iy5zIasBMValb3FWq6dPMew42Xid8TiZoa9ll/+NLo
BvPaGv1uEdPcbDS+HHzGCDeLunz0JScYnU4fJ3LnMhx4Go2SBywfp22BAQiogA4P
MKGulA4KDsOqhstv3bgFhweBlAgV3BDoYMZUFAYbp3izdkhxY6xqBvQdH8h4
-----END CERTIFICATE-----
)PEM";

static const char BOOT_DEVICE_KEY[] PROGMEM = R"PEM(
-----BEGIN RSA PRIVATE KEY-----
MIIEpAIBAAKCAQEAsCyydv72D0Dnr7NZlNrEtBBIvAeFqgxBPGe91BM5p+wvZCkY
KGwJcrtN1Z6GzL3Dpxtl5aC8Z+ri5t1Z64g1gu75qwnOK2V4yrVniTiRpHldGmjr
s5mjuErBSArJlUPky2ok1NT8cpQQOeTgsBEMMf2V2lZpwMcvCWlFTDftJCITSYts
SyXgeFgZLba1zOB1nfUcxPs5FS4FmqmcSJKu3dGc1YIluLBeMQTiTUmoPzF5Ibwb
J7wSkmt4c2FAiA5tDPvw1WBz3OV6Do1MTvshi42hOT4RvMneccIJQfymDDB8/Bac
lyKm4Q/4fWoc7eiigUJsod0JTbKAuh9HKRohbQIDAQABAoIBAAME0Q4O8mJyh0XQ
efF+Ci7XtJdcPaMbUKbbVDplFfDVsD40IhDXQRYxNXU6oS6y9b5QTt+vcGsLcFXq
ZxcNH3Wd6LNQmLLPq1E16I9GY+CAn+LK93b9Y/vNdCBq5udi4XHG4NiwyZ6hAOIt
FNiPgjj9EmtKXceK4jh0EFbtAmhDEe17zGMbHXJpSdpOqYf8PeyL4CA+Kg9bCUt9
pg3EjQYrpqFcz+lhvn+YUBEX1WXmwwV6LOLJtkCSXwpFrcWzU8QuwQRFTTEccXjS
yj5C57+UdyVnxFb6lCE+uXU0mzbU3VmHf4qMuWRHlp3nOUbgMMy1HiOFQp4hvNdA
9fswJzUCgYEA4G+dms4c8GCqWiYNRsZ9GcUyBpoPh/LL7hqMJ4ubIFRcBRGjqF6k
sBixeSbiVLX5S5iVzkN+dF2qtHDIcdlDvghsoMiAigFagwtXr0RkfCNtjoKVMCQg
lbDfduq5fnbtSw9zTmqGjeFAETa6E6DpALNY6ZupOYMz/TsAuViNDxsCgYEAyPOF
5cx6kHT5mNMdC/E9T1tt52ihw3sJuQEnXLUUGW244v024D4VvNXN5x6s+Gl/F155
4RMQePvZPvZZ4UoCp9W1kI69Ys+LSbOXZLksL6sY1z7FqZNv0DPPfZzGl6PVgSo4
1ONfs45KzEPCsCU6EGN5KYShM4xoaF8FpxWXshcCgYEAoGBB3/AvD7oToQ2V6SBH
HtYh1EPcm0ntXbzMsCl7dVOy2dBsARF4X3mVwDv/+YZ7mQB09AqY3zBbnksFgQ0+
8hl0/kOoTnV/JytAwjZ+E5MBY5d+YgM+4ZidZo1+dZcoI73rqHpIojP2oxZXS1Vt
ipmeDHi2KSnPScIS118mlUcCgYEAvhy2BqwnIFeqc4z3I/89E8DZ/14b/EaA8khH
JwwhUdVeX1FUA155TkewHwIEfpfAJym4gNl6chtCcrC6Y8uTK5mhxntkbQERM872
YdJWHVrvi2PVuRLFa+I4YQxbTccwDJ0XW0Epn0P1O763h9IQVsTlMgRKReyGMogD
XwmslE0CgYBfe/JAu2j5HuQKnQyP0IsJogz3yv7JEk408Xa1bm7sNreVAWi1rz2L
aesnedhaEUyAyKg6ILKw0+cIFcH4HXL5I5QTvOOFoDiaGZdJc9ie3XDvhwpJOJJq
YGtXg3qQrbam0faJ1D46Lv1ou1QWXhTNsTChSGgfu8OUVx8iTNd1AA==
-----END RSA PRIVATE KEY-----
)PEM";

// -----------------------------------------------------------------------------
// Amazon Root CA 1
// -----------------------------------------------------------------------------
static const char ROOT_CA[] PROGMEM = R"PEM(-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----)PEM";

String deviceCert, deviceKey, thingName;
bool mqttConnected       = false;
bool mqttServiceStarted   = false;
bool mqttClientAcquired  = false;
int  lastLimitState      = -1;
int  lastHallState       = -1;
uint32_t lastAttempt     = 0;
uint32_t lastTelemetry   = 0;

// -----------------------------------------------------------------------------
// Helper Functions for AT Commands & Serial
// -----------------------------------------------------------------------------
String readAvailable(uint32_t waitMs = 300) {
  String s; 
  uint32_t start = millis(), last = millis();
  while (millis() - start < waitMs) {
    while (Modem.available()) { 
      char c = (char)Modem.read(); 
      s += c; 
      last = millis(); 
    }
    if (s.length() && millis() - last > 100) break;
    delay(2);
  }
  return s;
}

static String pendingRxBuffer;

String sendAT(const String &cmd, uint32_t timeout = 3000) {
  while (Modem.available()) Modem.read();
  Serial.print("AT> "); Serial.println(cmd);
  Modem.print(cmd); Modem.print("\r\n");
  String out; 
  uint32_t start = millis();
  while (millis() - start < timeout) {
    while (Modem.available()) { 
      char c = (char)Modem.read(); 
      out += c; 
    }
    if (out.indexOf("\r\nOK\r\n") >= 0 || out.endsWith("\nOK\r\n") || 
        out.indexOf("ERROR") >= 0 || out.indexOf("+CME ERROR") >= 0) break;
    delay(2);
  }
  Serial.print("AT< "); Serial.println(out);

  int rxPos = out.indexOf("+CMQTTRX");
  if (rxPos >= 0) {
    pendingRxBuffer += out.substring(rxPos);
  }

  return out;
}

bool ok(const String &s) { 
  return s.indexOf("OK") >= 0 && s.indexOf("ERROR") < 0; 
}

String jsonEscape(const String &s) { 
  String o; 
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i]; 
    if (c == '\\' || c == '"') o += '\\'; 
    if (c == '\n') o += "\\n"; 
    else if (c != '\r') o += c;
  } 
  return o; 
}

String jsonGet(const String &j, const String &key) {
  String needle = "\"" + key + "\""; 
  int p = j.indexOf(needle); 
  if (p < 0) return "";
  p = j.indexOf(':', p + needle.length()); 
  if (p < 0) return ""; 
  p++;
  while (p < j.length() && (j[p] == ' ' || j[p] == '\r' || j[p] == '\n')) p++;
  if (p >= j.length() || j[p] != '"') return ""; 
  p++;
  String out; 
  out.reserve(j.length() > 256 ? 2048 : 64);
  bool esc = false;
  for (; p < j.length(); p++) {
    char c = j[p]; 
    if (esc) { 
      if (c == 'n') out += '\n'; 
      else if (c == 'r') out += '\r'; 
      else out += c; 
      esc = false; 
    } else if (c == '\\') {
      esc = true; 
    } else if (c == '"') {
      break; 
    } else {
      out += c; 
    }
  }
  return out;
}

String serialNumber() { 
  return String(FACTORY_ID); 
}
String makeClientId() { 
  if (thingName.length() > 0) return thingName;
  return String(FACTORY_ID); 
}

// -----------------------------------------------------------------------------
// Certificate Management
// -----------------------------------------------------------------------------
bool downloadToModem(const char *filename, const char *data) {
  sendAT(String("AT+CCERTDELE=\"") + filename + "\"", 2000);
  size_t len = strlen_P(data);
  String cmd = String("AT+CCERTDOWN=\"") + filename + "\"," + String((unsigned long)len);
  while (Modem.available()) Modem.read();
  Modem.println(cmd);
  String r = readAvailable(4000);
  Serial.print(r);
  if (r.indexOf('>') < 0 && r.indexOf("CONNECT") < 0) return false;
  
  const char *p = (const char*)data; 
  size_t sent = 0; 
  uint8_t buf[128];
  while (sent < len) { 
    size_t n = min((size_t)sizeof(buf), len - sent); 
    memcpy_P(buf, p + sent, n); 
    Modem.write(buf, n); 
    sent += n; 
    delay(2); 
  }
  r = readAvailable(12000); 
  Serial.print(r);
  return r.indexOf("OK") >= 0 && r.indexOf("ERROR") < 0;
}

bool downloadRamToModem(const char *filename, const String &data) {
  sendAT(String("AT+CCERTDELE=\"") + filename + "\"", 2000);
  String cmd = String("AT+CCERTDOWN=\"") + filename + "\"," + String(data.length());
  while (Modem.available()) Modem.read();
  Modem.println(cmd);
  String r = readAvailable(4000);
  Serial.print(r);
  if (r.indexOf('>') < 0 && r.indexOf("CONNECT") < 0) return false;
  
  size_t total = data.length();
  size_t sent = 0;
  while (sent < total) {
    size_t chunkSize = min((size_t)128, total - sent);
    Modem.write((const uint8_t*)(data.c_str() + sent), chunkSize);
    sent += chunkSize;
    delay(2);
  }
  r = readAvailable(12000);
  Serial.print(r);
  return r.indexOf("OK") >= 0 && r.indexOf("ERROR") < 0;
}

// -----------------------------------------------------------------------------
// TLS Configuration (Includes SNI and TLS 1.2+ for AWS IoT)
// -----------------------------------------------------------------------------
bool configureTLS(const char *certFile, const char *keyFile) {
  bool good = true;
  good &= ok(sendAT("AT+CSSLCFG=\"sslversion\",0,4"));        // Auto / TLS 1.2+
  good &= ok(sendAT("AT+CSSLCFG=\"authmode\",0,2"));          // Two-way mutual auth (server + client cert)
  good &= ok(sendAT("AT+CSSLCFG=\"ignorelocaltime\",0,1"));   // Avoid rejection due to uncalibrated RTC
  good &= ok(sendAT("AT+CSSLCFG=\"enableSNI\",0,1"));         // CRITICAL: AWS IoT requires SNI
  good &= ok(sendAT(String("AT+CSSLCFG=\"cacert\",0,\"") + ROOT_CA_FILE + "\""));
  good &= ok(sendAT(String("AT+CSSLCFG=\"clientcert\",0,\"") + certFile + "\""));
  good &= ok(sendAT(String("AT+CSSLCFG=\"clientkey\",0,\"") + keyFile + "\""));
  return good;
}

// -----------------------------------------------------------------------------
// Cellular & IP Network Bringup (Includes AT+NETOPEN for A7672S)
// -----------------------------------------------------------------------------
bool ensureCellular() {
  String r = sendAT("AT", 3000);
  if (!ok(r)) { 
    Serial.println("[SIM][FAIL] Modem not responding; check power and UART pins."); 
    return false; 
  }
  sendAT("ATE0", 2000);
  sendAT("AT+CMEE=2", 2000);

  // Check signal quality
  sendAT("AT+CSQ", 2000);

  // Check SIM card readiness
  bool simReady = false;
  for (int i = 0; i < 6; ++i) {
    r = sendAT("AT+CPIN?", 3000);
    if (r.indexOf("+CPIN: READY") >= 0) { 
      simReady = true; 
      break; 
    }
    Serial.println("[SIM][WAIT] SIM not READY; waiting 5 seconds...");
    delay(5000);
  }
  if (!simReady) { 
    Serial.println("[SIM][FAIL] SIM card not READY."); 
    return false; 
  }

  // Check network registration
  sendAT("AT+CREG?", 2000);
  sendAT("AT+CEREG?", 2000);

  // Set APN PDP context
  r = sendAT(String("AT+CGDCONT=1,\"IP\",\"") + APN + "\"", 5000);
  if (!ok(r)) { 
    Serial.println("[SIM][FAIL] APN profile rejected."); 
    return false; 
  }

  // Attach packet service
  bool attached = false;
  for (int i = 0; i < 18; ++i) {
    r = sendAT("AT+CGATT?", 3000);
    if (r.indexOf("+CGATT: 1") >= 0) { 
      Serial.println("[SIM][OK] Packet service attached."); 
      attached = true;
      break; 
    }
    if (i == 2) sendAT("AT+CGATT=1", 10000);
    Serial.println("[SIM][WAIT] Waiting for packet attachment...");
    delay(3000);
  }
  if (!attached) {
    Serial.println("[SIM][FAIL] Packet attachment timeout.");
    return false;
  }

  // Open network socket service (NETOPEN)
  r = sendAT("AT+NETOPEN?", 3000);
  if (r.indexOf("+NETOPEN: 1") < 0) {
    Serial.println("[NET] Opening IP data bearer (AT+NETOPEN)...");
    r = sendAT("AT+NETOPEN", 10000);
    if (r.indexOf("+NETOPEN: 0") < 0 && r.indexOf("already opened") < 0) {
      uint32_t t = millis();
      while (millis() - t < 10000) {
        while (Modem.available()) {
          char c = (char)Modem.read();
          r += c;
        }
        if (r.indexOf("+NETOPEN: 0") >= 0 || r.indexOf("already opened") >= 0) break;
        delay(20);
      }
    }
  }

  // Query assigned local IP address to confirm IP stack is active
  r = sendAT("AT+IPADDR", 5000);
  Serial.print("[NET] IP Address: "); Serial.println(r);

  return true;
}

// -----------------------------------------------------------------------------
// Disconnect Helper
// -----------------------------------------------------------------------------
void mqttDisconnect() {
  if (mqttConnected) {
    sendAT("AT+CMQTTDISC=0,10", 3000);
    mqttConnected = false;
  }
  if (mqttClientAcquired) {
    sendAT("AT+CMQTTREL=0", 2000);
    mqttClientAcquired = false;
  }
}

// -----------------------------------------------------------------------------
// MQTT Service & Client Initialization (Robust Lifecycle)
// -----------------------------------------------------------------------------
bool mqttStartClient() {
  Serial.println("[MQTT] Ensuring modem MQTT service/client state...");

  // 1. Ensure MQTT service is started on modem
  String r = sendAT("AT+CMQTTSTART", 5000);
  if (ok(r) || r.indexOf("+CMQTTSTART: 0") >= 0) {
    mqttServiceStarted = true;
  } else if (r.indexOf("23") >= 0 || r.indexOf("ERROR") >= 0) {
    mqttServiceStarted = true;
  }

  // 2. Always release any prior client 0 instance so we acquire fresh
  sendAT("AT+CMQTTDISC=0,10", 2000);
  sendAT("AT+CMQTTREL=0", 2000);
  mqttClientAcquired = false;

  // 3. Acquire client 0 with server_type = 1 (SSL/TLS)
  String clientId = makeClientId();
  r = sendAT(String("AT+CMQTTACCQ=0,\"") + clientId + "\",1", 5000);
  if (!ok(r)) {
    Serial.println("[MQTT][FAIL] Could not acquire MQTT client 0 with SSL.");
    return false;
  }
  mqttClientAcquired = true;

  // 4. Bind SSL context 0 to MQTT client 0
  if (!ok(sendAT("AT+CMQTTSSLCFG=0,0", 5000))) {
    Serial.println("[MQTT][FAIL] Could not bind SSL context 0 to MQTT client 0.");
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------
// Connect to AWS IoT Broker
// -----------------------------------------------------------------------------
bool mqttConnect(bool ssl) {
  Serial.println(ssl ? "[MQTT] Connecting to AWS IoT Core over TLS on port 8883..." : "[MQTT] Connecting without TLS...");
  
  String url = String("tcp://") + AWS_HOST + (ssl ? ":8883" : ":1883");
  String cmd = String("AT+CMQTTCONNECT=0,\"") + url + "\",60,1";
  String r = sendAT(cmd, 30000);
  if (r.indexOf("ERROR") >= 0 || r.indexOf("+CME ERROR") >= 0) {
    Serial.println("[MQTT][FAIL] CMQTTCONNECT command rejected immediately.");
    mqttConnected = false;
    mqttClientAcquired = false;
    return false;
  }
  if (r.indexOf("+CMQTTCONNECT: 0,0") >= 0) { 
    mqttConnected = true; 
    Serial.println("[MQTT][SUCCESS] Connected to AWS IoT broker!");
    return true; 
  }

  // Wait for asynchronous URC from modem
  uint32_t started = millis();
  String urc;
  while (millis() - started < 30000) {
    while (Modem.available()) {
      char c = (char)Modem.read();
      urc += c;
    }
    if (urc.indexOf("+CMQTTCONNECT: 0,0") >= 0) {
      Serial.println(urc); 
      mqttConnected = true; 
      Serial.println("[MQTT][SUCCESS] Connected to AWS IoT broker!");
      return true; 
    }
    if (urc.indexOf("+CMQTTCONNECT:") >= 0 && urc.indexOf(",0") < 0) {
      Serial.println(urc);
      Serial.println("[MQTT][FAIL] Connection rejected by broker (Check certs, policy, client ID, and SNI).");
      mqttConnected = false;
      mqttClientAcquired = false;
      break;
    }
    delay(20);
  }
  if (!mqttConnected) {
    Serial.println("[MQTT][FAIL] No successful +CMQTTCONNECT: 0,0 received within timeout.");
    mqttClientAcquired = false;
  }
  return mqttConnected;
}

// -----------------------------------------------------------------------------
// MQTT Publish (Standard SIMCom 3-Step Sequence: TOPIC -> PAYLOAD -> PUB)
// -----------------------------------------------------------------------------
bool mqttPublish(const String &topic, const String &payload) {
  if (!mqttConnected) {
    Serial.println("[MQTT][PUB][SKIP] MQTT is not connected.");
    return false;
  }

  // Step 1: Set Topic
  while (Modem.available()) Modem.read();
  String cmdTopic = String("AT+CMQTTTOPIC=0,") + topic.length();
  Serial.print("AT> "); Serial.println(cmdTopic);
  Modem.print(cmdTopic); Modem.print("\r\n");

  uint32_t t = millis();
  bool gotPrompt = false;
  while (millis() - t < 3000) {
    while (Modem.available()) {
      char c = (char)Modem.read();
      if (c == '>') { gotPrompt = true; break; }
    }
    if (gotPrompt) break;
    delay(2);
  }
  if (!gotPrompt) {
    Serial.println("[MQTT][PUB][FAIL] No '>' prompt for topic.");
    mqttConnected = false;
    mqttClientAcquired = false;
    return false;
  }
  Modem.print(topic);
  String res = readAvailable(3000);
  Serial.print("AT< "); Serial.println(res);
  if (!ok(res)) {
    Serial.println("[MQTT][PUB][FAIL] Topic not accepted.");
    return false;
  }

  // Step 2: Set Payload
  while (Modem.available()) Modem.read();
  String cmdPayload = String("AT+CMQTTPAYLOAD=0,") + payload.length();
  Serial.print("AT> "); Serial.println(cmdPayload);
  Modem.print(cmdPayload); Modem.print("\r\n");

  t = millis();
  gotPrompt = false;
  while (millis() - t < 3000) {
    while (Modem.available()) {
      char c = (char)Modem.read();
      if (c == '>') { gotPrompt = true; break; }
    }
    if (gotPrompt) break;
    delay(2);
  }
  if (!gotPrompt) {
    Serial.println("[MQTT][PUB][FAIL] No '>' prompt for payload.");
    mqttConnected = false;
    mqttClientAcquired = false;
    return false;
  }
  Modem.print(payload);
  res = readAvailable(3000);
  Serial.print("AT< "); Serial.println(res);
  if (!ok(res)) {
    Serial.println("[MQTT][PUB][FAIL] Payload not accepted.");
    return false;
  }

  // Step 3: Trigger Publish (QoS 0: instant delivery to broker, no PUBACK stall)
  String cmdPub = "AT+CMQTTPUB=0,0,60";
  res = sendAT(cmdPub, 10000);
  if (res.indexOf("ERROR") >= 0 || res.indexOf("+CMQTTCONNLOST") >= 0) {
    Serial.println("[MQTT][PUB][FAIL] Publish failed or connection lost.");
    mqttConnected = false;
    mqttClientAcquired = false;
    return false;
  }

  if (res.indexOf("+CMQTTPUB: 0,0") >= 0) {
    Serial.println("[MQTT][PUB][SUCCESS] Message published successfully!");
    return true;
  }
  
  String urc = readAvailable(5000);
  Serial.print(urc);
  int rxPos = urc.indexOf("+CMQTTRX");
  if (rxPos >= 0) {
    pendingRxBuffer += urc.substring(rxPos);
  }
  if (urc.indexOf("+CMQTTCONNLOST") >= 0) {
    mqttConnected = false;
    mqttClientAcquired = false;
    return false;
  }
  if (urc.indexOf("+CMQTTPUB: 0,0") >= 0) {
    Serial.println("[MQTT][PUB][SUCCESS] Message published successfully!");
    return true;
  }

  return (res.indexOf("OK") >= 0);
}

// -----------------------------------------------------------------------------
// MQTT Subscribe (Standard SIMCom SUBTOPIC -> SUB)
// -----------------------------------------------------------------------------
bool mqttSubscribe(const String &topic) {
  Serial.println(String("[MQTT][SUB] Subscribing to: ") + topic);
  while (Modem.available()) Modem.read();
  String cmd = String("AT+CMQTTSUBTOPIC=0,") + topic.length() + ",1";
  Serial.print("AT> "); Serial.println(cmd);
  Modem.print(cmd); Modem.print("\r\n");

  uint32_t t = millis();
  bool gotPrompt = false;
  while (millis() - t < 3000) {
    while (Modem.available()) {
      char c = (char)Modem.read();
      if (c == '>') { gotPrompt = true; break; }
    }
    if (gotPrompt) break;
    delay(2);
  }
  if (!gotPrompt) return false;
  Modem.print(topic);
  String res = readAvailable(3000);
  Serial.print("AT< "); Serial.println(res);
  if (!ok(res)) return false;

  String r = sendAT("AT+CMQTTSUB=0", 15000);
  return ok(r) || r.indexOf("+CMQTTSUB: 0,0") >= 0;
}

// -----------------------------------------------------------------------------
// Fleet Provisioning JSON Wait Helper (High Performance UART Drain)
// -----------------------------------------------------------------------------
String waitForJson(uint32_t timeoutMs) {
  String all = pendingRxBuffer; 
  pendingRxBuffer = "";
  all.reserve(16384);

  uint32_t start = millis();
  uint32_t lastByteTime = millis();
  bool rxStarted = (all.length() > 0);

  Serial.println("[JSON] Awaiting AWS response stream...");

  while (millis() - start < timeoutMs) {
    while (Modem.available()) {
      char c = (char)Modem.read();
      all += c;
      lastByteTime = millis();
      rxStarted = true;
    }

    if (rxStarted) {
      // 1. Packet complete indicator from modem
      if (all.indexOf("+CMQTTRXEND") >= 0) {
        break;
      }
      // 2. Or closing brace present and no more bytes for 350ms
      if ((millis() - lastByteTime > 350) && all.indexOf('{') >= 0 && all.lastIndexOf('}') > all.indexOf('{')) {
        break;
      }
    }
    delay(2);
  }

  // Parse JSON candidate once stream is complete
  int a = all.indexOf('{');
  int end = all.lastIndexOf('}');
  if (a >= 0 && end > a) { 
    String candidate = all.substring(a, end + 1);
    if (candidate.indexOf("certificateOwnershipToken") >= 0 ||
        candidate.indexOf("certificatePem") >= 0 ||
        candidate.indexOf("thingName") >= 0 ||
        candidate.indexOf("errorMessage") >= 0) {
      return candidate;
    }
  }

  Serial.println("[JSON][WARN] Incomplete or timed out response. Buffer length: " + String(all.length()));
  if (all.length() > 0) {
    Serial.println(all);
  }
  return "";
}

// -----------------------------------------------------------------------------
// Fleet Provisioning Routines (Creates Thing in AWS IoT Core)
// -----------------------------------------------------------------------------
bool fleetProvision() {
  Serial.println("==================================================");
  Serial.println("[PROV] Starting AWS IoT Fleet Provisioning...");
  Serial.println("==================================================");

  // 1. Download / Verify claim certs on modem
  String certList = sendAT("AT+CCERTLIST", 3000);
  if (certList.indexOf(ROOT_CA_FILE) < 0) downloadToModem(ROOT_CA_FILE, ROOT_CA);
  if (certList.indexOf(CLAIM_CERT_FILE) < 0) downloadToModem(CLAIM_CERT_FILE, CLAIM_CERT);
  if (certList.indexOf(CLAIM_KEY_FILE) < 0) downloadToModem(CLAIM_KEY_FILE, CLAIM_KEY);

  // 2. Configure TLS for claim session
  if (!configureTLS(CLAIM_CERT_FILE, CLAIM_KEY_FILE)) {
    Serial.println("[PROV][FAIL] TLS config failed for claim session.");
    return false;
  }

  // 3. Connect to AWS IoT Core as claim client
  if (!mqttStartClient() || !mqttConnect(true)) {
    Serial.println("[PROV][FAIL] Failed to connect to AWS with claim certificate.");
    return false;
  }

  // 4. Subscribe to certificate creation and provisioning topics
  Serial.println("[PROV] Subscribing to AWS provisioning topics...");
  bool s1 = mqttSubscribe("$aws/certificates/create/json/accepted"); delay(400);
  bool s2 = mqttSubscribe("$aws/certificates/create/json/rejected"); delay(400);
  bool s3 = mqttSubscribe(String("$aws/provisioning-templates/") + TEMPLATE_NAME + "/provision/json/accepted"); delay(400);
  bool s4 = mqttSubscribe(String("$aws/provisioning-templates/") + TEMPLATE_NAME + "/provision/json/rejected"); delay(400);
  if (!s1 || !s2 || !s3 || !s4) {
    Serial.println("[PROV][FAIL] Failed to subscribe to all provisioning topics.");
    mqttDisconnect();
    return false;
  }

  // 5. Request new certificate from AWS
  Serial.println("[PROV] Requesting new device certificate from AWS ($aws/certificates/create/json)...");
  delay(1000);
  if (!mqttPublish("$aws/certificates/create/json", "{}")) {
    Serial.println("[PROV][FAIL] Failed to publish to create certificate topic.");
    mqttDisconnect();
    return false;
  }

  // 6. Wait for response from $aws/certificates/create/json/accepted
  String j = waitForJson(25000);
  if (j.length() == 0) {
    Serial.println("[PROV][FAIL] Timeout waiting for AWS certificate creation response.");
    mqttDisconnect();
    return false;
  }
  Serial.println("[PROV] AWS Certificate Creation Response:");
  Serial.println(j);

  String cert = jsonGet(j, "certificatePem");
  String key = jsonGet(j, "privateKey");
  String token = jsonGet(j, "certificateOwnershipToken");

  if (cert.length() < 100 || key.length() < 100 || token.length() < 20) {
    Serial.println("[PROV][FAIL] Certificate creation response missing required fields.");
    mqttDisconnect();
    return false;
  }
  Serial.println("[PROV][OK] New unique certificate and ownership token generated by AWS!");

  // 7. Register Thing via Provisioning Template
  Serial.println("[PROV] Executing Provisioning Template to create Thing in AWS IoT Core...");
  String registerPayload = String("{\"certificateOwnershipToken\":\"") + jsonEscape(token) + 
                          "\",\"parameters\":{\"SerialNumber\":\"" + jsonEscape(serialNumber()) + 
                          "\",\"MasterId\":\"" + jsonEscape(serialNumber()) + "\"}}";
  delay(1000);
  if (!mqttPublish(String("$aws/provisioning-templates/") + TEMPLATE_NAME + "/provision/json", registerPayload)) {
    Serial.println("[PROV][FAIL] Failed to publish register payload.");
    mqttDisconnect();
    return false;
  }

  // 8. Wait for register confirmation
  String pr = waitForJson(25000);
  Serial.println("[PROV] Provisioning Registration Response:");
  Serial.println(pr);

  thingName = jsonGet(pr, "thingName");
  if (thingName.length() == 0) {
    if (pr.indexOf("rejected") >= 0 || pr.indexOf("errorMessage") >= 0) {
      Serial.println("[PROV][FAIL] Provisioning template rejected registration.");
      mqttDisconnect();
      return false;
    }
    thingName = String(FACTORY_ID);
  }

  // 9. Save newly provisioned credentials to ESP32 Flash (NVS Preferences)
  prefs.begin("lcgate", false);
  prefs.putString("cert", cert);
  prefs.putString("key", key);
  prefs.putString("thing", thingName);
  prefs.end();

  deviceCert = cert;
  deviceKey  = key;

  // Write new permanent cert & key to modem filesystem
  downloadRamToModem(DEVICE_CERT_FILE, deviceCert);
  downloadRamToModem(DEVICE_KEY_FILE, deviceKey);

  Serial.println("==================================================");
  Serial.printf("[PROV][SUCCESS] THING CREATED IN AWS IOT CORE: %s\n", thingName.c_str());
  Serial.println("==================================================");

  // 10. Disconnect claim session
  mqttDisconnect();
  delay(1500);
  return true;
}

// -----------------------------------------------------------------------------
// Load Permanent Credentials (From NVS)
// -----------------------------------------------------------------------------
bool loadPermanent() {
  prefs.begin("lcgate", true); 
  deviceCert = prefs.getString("cert", ""); 
  deviceKey  = prefs.getString("key", ""); 
  thingName  = prefs.getString("thing", ""); 
  prefs.end();

  if (deviceCert.length() > 100 && deviceKey.length() > 100) {
    if (thingName.length() == 0) thingName = String(FACTORY_ID);
    Serial.printf("[AWS] Found provisioned Thing in NVS: %s\n", thingName.c_str());
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
// Connect Permanent Device Session
// -----------------------------------------------------------------------------
bool connectPermanent() {
  Serial.println("[AWS] Setting up permanent device TLS session...");
  
  // Verify certificates exist in modem flash
  String certList = sendAT("AT+CCERTLIST", 3000);
  if (certList.indexOf(ROOT_CA_FILE) < 0) {
    if (!downloadToModem(ROOT_CA_FILE, ROOT_CA)) return false;
  }
  if (certList.indexOf(DEVICE_CERT_FILE) < 0) {
    if (!downloadRamToModem(DEVICE_CERT_FILE, deviceCert)) return false;
  }
  if (certList.indexOf(DEVICE_KEY_FILE) < 0) {
    if (!downloadRamToModem(DEVICE_KEY_FILE, deviceKey)) return false;
  }

  if (!configureTLS(DEVICE_CERT_FILE, DEVICE_KEY_FILE)) return false;
  if (!mqttStartClient() || !mqttConnect(true)) return false;
  
  mqttConnected = true; 
  return true;
}

// -----------------------------------------------------------------------------
// Gate Sensor Reading & Telemetry Publishing (Option A: Dual Sensor Tracking)
// -----------------------------------------------------------------------------
bool isLimitClosed() {
  return digitalRead(LIMIT_PIN) == LOW;
}

bool isHallClosed() {
  return digitalRead(HALL_PIN) == LOW;
}

bool isClosed() { 
  return isLimitClosed() || isHallClosed(); 
}

void publishGate(const char *triggerReason = nullptr) {
  if (!mqttConnected) {
    Serial.println("[PUB][SKIP] MQTT is not connected.");
    return;
  }

  bool limitClosed = isLimitClosed();
  bool hallClosed  = isHallClosed();
  bool closed      = limitClosed || hallClosed;

  String sensorType;
  if (limitClosed && hallClosed) {
    sensorType = "Both";
  } else if (limitClosed) {
    sensorType = "LimitSwitch";
  } else if (hallClosed) {
    sensorType = "HallEffect";
  } else {
    sensorType = "None";
  }

  String id = thingName.length() ? thingName : String(FACTORY_ID);
  String topic = String("lc-gate/") + id + "/telemetry";
  String state = closed ? "CLOSED" : "NOT_CLOSED";
  String payload = String("{\"MasterDeviceId\":\"") + id + 
                          "\",\"DeviceType\":\"LC_GATE_MASTER\"" +
                          ",\"Status\":\"" + state + "\"" +
                          ",\"SensorType\":\"" + sensorType + "\"" +
                          ",\"LimitSwitchClosed\":" + (limitClosed ? "true" : "false") + 
                          ",\"HallClosed\":" + (hallClosed ? "true" : "false") + 
                          ",\"TimeStampMs\":" + String(millis()) + "}";
  if (triggerReason != nullptr) {
    Serial.printf("[EVENT] Gate trigger: %s (Sensor: %s, State: %s)\n", triggerReason, sensorType.c_str(), state.c_str());
  }
  bool teleOk = mqttPublish(topic, payload);
  Serial.printf("[PUBLISH] telemetry=%s\n", teleOk ? "SUCCESS" : "FAILED");
  Serial.println(payload);
}

// -----------------------------------------------------------------------------
// Arduino setup()
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); 
  delay(1000);

  pinMode(LIMIT_PIN, INPUT_PULLUP); 
  pinMode(HALL_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("==========================================================");
  Serial.println("  LC Gate Master Firmware - ESP32 + SIMCom A7672S         ");
  Serial.println("  AWS IoT Core Cloud Connectivity & Fleet Provisioning   ");
  Serial.println("==========================================================");
  Serial.printf("Factory ID: %s\n", FACTORY_ID);
  Serial.printf("Target APN: %s\n", APN);
  Serial.printf("AWS Host:   %s\n", AWS_HOST);

  Modem.setRxBufferSize(16384); // Expand RX buffer to 16KB to safely hold large AWS TLS certificates
  Modem.begin(115200, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN); 
  delay(1500);

  // 1. Bring up cellular network & open IP socket bearer (NETOPEN)
  if (!ensureCellular()) { 
    Serial.println("[INIT][FAIL] Cellular setup failed; will retry in 15 seconds."); 
    lastAttempt = millis(); 
    return; 
  }

  // 2. Check if Thing is already provisioned in NVS Flash
  bool provisioned = loadPermanent();
  if (!provisioned) {
    Serial.println("[PROV] No provisioned Thing found in NVS. Starting Fleet Provisioning...");
    if (fleetProvision()) { 
      provisioned = true;
    } else {
      Serial.println("[PROV][FALLBACK] Fleet provisioning did not finish. Falling back to direct device certificate...");
      deviceCert = String(FPSTR(BOOT_DEVICE_CERT));
      deviceKey  = String(FPSTR(BOOT_DEVICE_KEY));
      thingName  = String(FACTORY_ID);
    }
  }

  // 3. Connect to AWS IoT Core over TLS as the Thing
  if (!connectPermanent()) { 
    Serial.println("[AWS][FAIL] Permanent AWS TLS/MQTT connect failed; will retry in 15s."); 
    lastAttempt = millis();
    return; 
  }

  // 4. Initial state publish
  lastLimitState = isLimitClosed() ? 1 : 0;
  lastHallState  = isHallClosed() ? 1 : 0;
  publishGate("InitialState");
  lastTelemetry = millis();
}

// -----------------------------------------------------------------------------
// Arduino loop()
// -----------------------------------------------------------------------------
void loop() {
  // Reconnect check every 15 seconds if connection is lost
  if (!mqttConnected && (millis() - lastAttempt >= 15000)) {
    lastAttempt = millis();
    Serial.println("[LOOP] Reconnecting to AWS IoT Core...");
    if (ensureCellular()) {
      if (loadPermanent()) {
        connectPermanent();
      } else {
        fleetProvision();
      }
    }
  }

  // Periodic heartbeat every 60 seconds while connected
  if (mqttConnected && (millis() - lastTelemetry >= 60000)) {
    lastTelemetry = millis();
    publishGate("PeriodicHeartbeat");
  }

  // Event-driven gate sensor state changes with debouncing
  bool curLimit = isLimitClosed();
  bool curHall  = isHallClosed();
  int limitState = curLimit ? 1 : 0;
  int hallState  = curHall ? 1 : 0;

  if ((limitState != lastLimitState || hallState != lastHallState) && mqttConnected) {
    delay(80); // Debounce delay
    bool stableLimit = isLimitClosed();
    bool stableHall  = isHallClosed();
    int stableLimitState = stableLimit ? 1 : 0;
    int stableHallState  = stableHall ? 1 : 0;

    if (stableLimitState != lastLimitState || stableHallState != lastHallState) {
      lastLimitState = stableLimitState;
      lastHallState  = stableHallState;
      
      const char *reason = "SensorStateChanged";
      if (stableLimit && stableHall) {
        reason = "BothSensorsActive";
      } else if (stableLimit) {
        reason = "LimitSwitchActive";
      } else if (stableHall) {
        reason = "HallEffectActive";
      } else {
        reason = "GateOpened";
      }
      publishGate(reason);
    }
  }
  delay(50);
}
