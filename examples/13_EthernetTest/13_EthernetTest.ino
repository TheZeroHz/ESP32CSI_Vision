/**
 * 13_EthernetTest — onboard 100M Ethernet (IP101) on Guition JC-ESP32P4-M3
 *
 * Plug an Ethernet cable into a DHCP router/switch, open Serial @ 115200.
 * You should see Link Up → Got IP → periodic HTTP GET to example.com.
 *
 * Board pins (IP101GRI / RMII):
 *   MDC=31  MDIO=52  PHY power/reset=51  REF_CLK in=50  PHY addr=1
 *   RMII data pins are EMAC defaults (TXD0=34, TXD1=35, TX_EN=49,
 *   RXD0=30, RXD1=29, CRS_DV=28).
 *
 * Arduino-ESP32 3.x + board: ESP32P4 Dev Module (PSRAM Enabled).
 */

#include <Arduino.h>

// Must be defined BEFORE including ETH.h so ETH.begin() uses these defaults.
#ifndef ETH_PHY_MDC
#define ETH_PHY_TYPE  ETH_PHY_IP101
#define ETH_PHY_ADDR  1
#define ETH_PHY_MDC   31
#define ETH_PHY_MDIO  52
#define ETH_PHY_POWER 51
#define ETH_CLK_MODE  EMAC_CLK_EXT_IN
#endif

#include <ETH.h>
#include <NetworkClient.h>

static volatile bool eth_connected = false;

// Called from a FreeRTOS task — keep it short; no delay().
void onEthEvent(arduino_event_id_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("guition-p4-m3");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Link Up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.println("ETH Got IP");
      Serial.println(ETH);
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println("ETH Lost IP");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Link Down");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

static void testHttp(const char *host, uint16_t port) {
  Serial.printf("\nHTTP GET http://%s:%u/\n", host, (unsigned)port);

  NetworkClient client;
  client.setTimeout(5000);
  if (!client.connect(host, port)) {
    Serial.println("connect FAILED (DNS/route/firewall?)");
    return;
  }

  client.printf("GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", host);

  uint32_t t0 = millis();
  while (client.connected() && !client.available() && millis() - t0 < 5000) {
    delay(10);
  }

  size_t n = 0;
  while (client.available() && n < 512) {
    Serial.write(client.read());
    n++;
  }
  Serial.printf("\n... (%u bytes shown)\n", (unsigned)n);
  client.stop();
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("=== 13_EthernetTest (Guition JC-ESP32P4-M3 / IP101) ===");
  Serial.println("Plug Ethernet cable, wait for DHCP...");

  Network.onEvent(onEthEvent);

  // Explicit begin — same pins as the #defines above.
  if (!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER,
                 ETH_CLK_MODE)) {
    Serial.println("ETH.begin FAILED — check PHY power/cable/pins");
    while (true) delay(1000);
  }
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last < 10000) return;
  last = millis();

  if (!eth_connected) {
    Serial.println("[wait] no Ethernet IP yet...");
    return;
  }

  Serial.printf("[ok] IP=%s  MAC=%s  link=%s\n", ETH.localIP().toString().c_str(),
                ETH.macAddress().c_str(), ETH.linkUp() ? "UP" : "DOWN");
  testHttp("example.com", 80);
}
