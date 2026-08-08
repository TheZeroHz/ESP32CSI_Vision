#include "WfmNetwork.h"

WfmNetwork *WfmNetwork::s_ethOwner = nullptr;

IPAddress WfmNetwork::localIP() const {
  if (_ready) return _ip;
  if (_kind == WfmNetKind::WiFiSTA) return WiFi.localIP();
  if (_kind == WfmNetKind::WiFiAP) return WiFi.softAPIP();
  if (_kind == WfmNetKind::Ethernet) return ETH.localIP();
  return IPAddress();
}

void WfmNetwork::notifyGotIP(IPAddress ip) {
  _ip = ip;
  _ready = true;
}

bool WfmNetwork::beginSTA(const char *ssid, const char *password, uint32_t timeoutMs) {
  _kind = WfmNetKind::WiFiSTA;
  _ready = false;
  WiFi.mode(WIFI_STA);
  if (_hostname) WiFi.setHostname(_hostname);
  WiFi.begin(ssid, password);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) return false;
    delay(200);
  }
  _ip = WiFi.localIP();
  _ready = true;
  return true;
}

bool WfmNetwork::beginAP(const char *ssid, const char *password, int channel) {
  _kind = WfmNetKind::WiFiAP;
  WiFi.mode(WIFI_AP);
  bool ok = password && password[0] ? WiFi.softAP(ssid, password, channel)
                                    : WiFi.softAP(ssid, nullptr, channel);
  if (!ok) return false;
  _ip = WiFi.softAPIP();
  _ready = true;
  return true;
}

void WfmNetwork::onEthEvent(arduino_event_id_t event, arduino_event_info_t info) {
  (void)info;
  if (!s_ethOwner) return;
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      ETH.setHostname(s_ethOwner->hostname());
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      s_ethOwner->_ip = ETH.localIP();
      s_ethOwner->_ready = true;
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    case ARDUINO_EVENT_ETH_STOP:
      s_ethOwner->_ready = false;
      break;
    default:
      break;
  }
}

bool WfmNetwork::beginEthernet(const WfmEthConfig &cfg) {
  _kind = WfmNetKind::Ethernet;
  _ready = false;
  s_ethOwner = this;
  Network.onEvent(onEthEvent);
  if (!ETH.begin(cfg.phyType, cfg.phyAddr, cfg.mdc, cfg.mdio, cfg.power, cfg.clkMode)) {
    return false;
  }
  return true;
}
