#include "panel_manager.h"
#include "display.h"

static constexpr unsigned long SPLASH_DURATION_MS    = 4000;
static constexpr unsigned long MENU_DURATION_MS      = 5000;
static constexpr unsigned long NETWORK_DURATION_MS   = 5000;
static constexpr unsigned long I2C_SCAN_DURATION_MS  = 5000;
static constexpr unsigned long RTC_STATUS_DURATION_MS = 5000;
static constexpr unsigned long ERROR_DURATION_MS     = 5000;
static constexpr unsigned long TIMED_PANEL_REFRESH_MS = 250;

PanelManager::PanelManager()
  : currentPanel(Panel::GRAPH),
    prevPanel(Panel::GRAPH),
    primaryPanel(Panel::GRAPH),
    panelUntil(0),
    lastRender(0),
    panelData(),
    queuedPanelCount(0) {}

unsigned long PanelManager::panelDuration(Panel panel) {
  switch (panel) {
    case Panel::SPLASH:        return SPLASH_DURATION_MS;
    case Panel::MENU:          return MENU_DURATION_MS;
    case Panel::NETWORK_INFO:  return NETWORK_DURATION_MS;
    case Panel::I2C_SCAN:      return I2C_SCAN_DURATION_MS;
    case Panel::RTC_STATUS:    return RTC_STATUS_DURATION_MS;
    case Panel::ERROR_MESSAGE: return ERROR_DURATION_MS;
    default:                   return 0;
  }
}

void PanelManager::activatePanel(Panel panel, unsigned long durationMs, const PanelPayload &data, unsigned long now) {
  prevPanel    = currentPanel;
  currentPanel = panel;
  panelUntil   = (durationMs == 0) ? 0 : now + durationMs;
  panelData    = data;
  lastRender   = 0;
}

bool PanelManager::enqueuePanelBack(Panel panel, unsigned long durationMs, const PanelPayload &data) {
  if (queuedPanelCount >= PANEL_QUEUE_CAPACITY) return false;
  queuedPanels[queuedPanelCount++] = {panel, durationMs, data};
  return true;
}

bool PanelManager::enqueuePanelFront(Panel panel, unsigned long durationMs, const PanelPayload &data) {
  if (queuedPanelCount >= PANEL_QUEUE_CAPACITY) return false;
  for (int i = queuedPanelCount; i > 0; --i) {
    queuedPanels[i] = queuedPanels[i - 1];
  }
  queuedPanels[0] = {panel, durationMs, data};
  queuedPanelCount++;
  return true;
}

bool PanelManager::dequeuePanel(PanelRequest &request) {
  if (queuedPanelCount == 0) return false;
  request = queuedPanels[0];
  for (uint8_t i = 1; i < queuedPanelCount; ++i) {
    queuedPanels[i - 1] = queuedPanels[i];
  }
  queuedPanelCount--;
  return true;
}

bool PanelManager::setPanel(Panel panel, const PanelPayload &data, unsigned long now) {
  const unsigned long durationMs = panelDuration(panel);
  bool dataChanged = !(panelData == data);

  if (panel == Panel::GRAPH || panel == Panel::HISTOGRAM) {
    primaryPanel = panel;
  }

  if (panel == currentPanel) {
    if (durationMs != 0)  panelUntil = now + durationMs;
    if (dataChanged)       panelData  = data;
    return false;
  }

  if (currentPanel == Panel::GRAPH || currentPanel == Panel::HISTOGRAM) {
    activatePanel(panel, durationMs, data, now);
    return true;
  }

  if (panelPriority(panel) > panelPriority(currentPanel) && panel == Panel::ERROR_MESSAGE) {
    unsigned long remainingMs = (panelUntil > now) ? (panelUntil - now) : 0;
    if (currentPanel != Panel::GRAPH) {
      enqueuePanelFront(currentPanel, remainingMs, panelData);
    }
    activatePanel(panel, durationMs, data, now);
    return true;
  }

  enqueuePanelBack(panel, durationMs, data);
  return false;
}

bool PanelManager::checkExpiration(unsigned long now) {
  if (panelUntil == 0 || now < panelUntil) return false;

  PanelRequest next;
  if (dequeuePanel(next)) {
    activatePanel(next.panel, next.durationMs, next.data, now);
  } else {
    prevPanel    = currentPanel;
    currentPanel = primaryPanel;
    panelUntil   = 0;
    lastRender   = 0;
  }
  return true;
}

bool PanelManager::shouldRenderTimedPanel(unsigned long now) const {
  return panelChanged() || now - lastRender >= TIMED_PANEL_REFRESH_MS;
}

unsigned long PanelManager::secondsRemaining(unsigned long now) const {
  return (panelUntil > now) ? ((panelUntil - now + 999UL) / 1000UL) : 0;
}

void PanelManager::markRendered(unsigned long now) {
  lastRender = now;
}

void PanelManager::render(U8G2 &u8g2, unsigned long now) {
  switch (currentPanel) {
    case Panel::GRAPH:
    case Panel::HISTOGRAM:
      markRendered(now);
      break;

    case Panel::SPLASH:
      if (shouldRenderTimedPanel(now)) {
        showSplash(u8g2, secondsRemaining(now));
        markRendered(now);
      }
      break;

    case Panel::MENU:
      if (shouldRenderTimedPanel(now)) {
        showMenu(u8g2, secondsRemaining(now));
        markRendered(now);
      }
      break;

    case Panel::NETWORK_INFO:
      if (shouldRenderTimedPanel(now)) {
        showNetworkInfo(u8g2, panelData.networkSsid, panelData.networkIp, secondsRemaining(now));
        markRendered(now);
      }
      break;

    case Panel::I2C_SCAN:
      if (shouldRenderTimedPanel(now)) {
        showI2CScan(u8g2, secondsRemaining(now));
        markRendered(now);
      }
      break;

    case Panel::RTC_STATUS:
      if (shouldRenderTimedPanel(now)) {
        showRtcStatus(u8g2, secondsRemaining(now));
        markRendered(now);
      }
      break;

    case Panel::ERROR_MESSAGE:
      if (shouldRenderTimedPanel(now)) {
        showErrorMessage(u8g2, panelData.errorMessage, secondsRemaining(now));
        markRendered(now);
      }
      break;
  }
}

int PanelManager::panelPriority(Panel panel) {
  switch (panel) {
    case Panel::ERROR_MESSAGE: return 20;
    case Panel::I2C_SCAN:      return 15;
    case Panel::RTC_STATUS:    return 12;
    case Panel::NETWORK_INFO:  return 10;
    case Panel::MENU:          return 5;
    case Panel::SPLASH:        return 1;
    default:                   return 0;
  }
}
