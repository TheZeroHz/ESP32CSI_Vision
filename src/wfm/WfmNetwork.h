#pragma once
/**
 * @file WfmNetwork.h
 * @brief WiFi STA / AP and Ethernet bring-up helpers for WebFileManager.
 */
#include <Arduino.h>
#include <IPAddress.h>
#include <WiFi.h>
#include <ETH.h>
#include <Network.h>

#define WFM_HAS_WIFI 1
#define WFM_HAS_ETH 1

enum class WfmNetKind : uint8_t { None = 0, WiFiSTA, WiFiAP, Ethernet };

/**
 * Pin / PHY bundle for Arduino-ESP32 Ethernet.
 * Note: EMAC_CLK_* are enum values (not macros) — do not use #ifdef on them.
 */
struct WfmEthConfig {
  eth_phy_type_t phyType = ETH_PHY_IP101;
  int32_t phyAddr = 1;
  int mdc = -1;
  int mdio = -1;
  int power = -1;
#if CONFIG_IDF_TARGET_ESP32P4
  eth_clock_mode_t clkMode = EMAC_CLK_EXT_IN;
#elif CONFIG_IDF_TARGET_ESP32
  eth_clock_mode_t clkMode = ETH_CLOCK_GPIO0_IN;
#else
  eth_clock_mode_t clkMode = EMAC_CLK_EXT_IN;
#endif
};

/**
 * Thin network facade — call one begin*() then poll ready() / localIP().
 */
class WfmNetwork {
 public:
  void setHostname(const char *name) { _hostname = name; }
  const char *hostname() const { return _hostname ? _hostname : "web-file-mgr"; }

  WfmNetKind kind() const { return _kind; }
  bool ready() const { return _ready; }
  IPAddress localIP() const;

  bool beginSTA(const char *ssid, const char *password, uint32_t timeoutMs = 20000);
  bool beginAP(const char *ssid, const char *password = "", int channel = 1);
  bool beginEthernet(const WfmEthConfig &cfg);

  /** Guition JC-ESP32P4-M3 IP101 (MDC=31 MDIO=52 POWER=51 CLK=ext). */
  static WfmEthConfig guitionM3Eth() {
    WfmEthConfig c;
    c.phyType = ETH_PHY_IP101;
    c.phyAddr = 1;
    c.mdc = 31;
    c.mdio = 52;
    c.power = 51;
#if CONFIG_IDF_TARGET_ESP32P4
    c.clkMode = EMAC_CLK_EXT_IN;
#endif
    return c;
  }

  void notifyGotIP(IPAddress ip);

 private:
  static void onEthEvent(arduino_event_id_t event, arduino_event_info_t info);
  static WfmNetwork *s_ethOwner;

  const char *_hostname = "web-file-mgr";
  WfmNetKind _kind = WfmNetKind::None;
  bool _ready = false;
  IPAddress _ip;
};
