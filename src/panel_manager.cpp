#include "panel_manager.h"

PanelManager::PanelManager()
  : currentPanel(Panel::GRAPH),
    prevPanel(Panel::GRAPH),
    panelUntil(0),
    lastRender(0),
    panelData(),
    queuedPanelCount(0),
    splashMs(4000),
    menuMs(5000),
    networkInfoMs(5000),
    i2cScanMs(5000),
    rtcStatusMs(5000),
    errorMessageMs(5000) {}

void PanelManager::activatePanel(Panel panel, unsigned long durationMs, const PanelPayload &data, unsigned long now) {
  prevPanel = currentPanel;
  currentPanel = panel;
  panelUntil = (durationMs == 0) ? 0 : now + durationMs;
  panelData = data;
  lastRender = 0;
}

bool PanelManager::enqueuePanelBack(Panel panel, unsigned long durationMs, const PanelPayload &data) {
  if (queuedPanelCount >= 6) {
    return false;
  }

  queuedPanels[queuedPanelCount++] = {panel, durationMs, data};
  return true;
}

bool PanelManager::enqueuePanelFront(Panel panel, unsigned long durationMs, const PanelPayload &data) {
  if (queuedPanelCount >= 6) {
    return false;
  }

  for (int index = queuedPanelCount; index > 0; --index) {
    queuedPanels[index] = queuedPanels[index - 1];
  }
  queuedPanels[0] = {panel, durationMs, data};
  queuedPanelCount++;
  return true;
}

bool PanelManager::dequeuePanel(PanelRequest &request) {
  if (queuedPanelCount == 0) {
    return false;
  }

  request = queuedPanels[0];
  for (uint8_t index = 1; index < queuedPanelCount; ++index) {
    queuedPanels[index - 1] = queuedPanels[index];
  }
  queuedPanelCount--;
  return true;
}

bool PanelManager::setPanel(Panel panel, unsigned long durationMs, const PanelPayload &data, unsigned long now) {
  bool dataChanged = !(panelData == data);

  // If same panel, just update duration and data if needed
  if (panel == currentPanel) {
    if (durationMs != 0) {
      panelUntil = now + durationMs;
    }
    if (dataChanged) {
      panelData = data;
    }
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
  if (panelUntil != 0 && now >= panelUntil) {
    PanelRequest nextRequest;
    if (dequeuePanel(nextRequest)) {
      activatePanel(nextRequest.panel, nextRequest.durationMs, nextRequest.data, now);
    } else {
      prevPanel = currentPanel;
      currentPanel = nextPanel(currentPanel);
      panelUntil = 0;
      lastRender = 0;
    }
    return true;
  }
  return false;
}

void PanelManager::render(U8G2 &u8g2, unsigned long now) {
  bool panelChanged = (currentPanel != prevPanel);

  switch (currentPanel) {
    case Panel::GRAPH:
    case Panel::HISTOGRAM:
      // Graph rendering is handled externally; just mark as rendered
      lastRender = now;
      break;

    case Panel::SPLASH:
      if (panelChanged || now - lastRender >= 250UL) {
        unsigned long remainingSecs = (panelUntil > now) ? ((panelUntil - now + 999UL) / 1000UL) : 0;
        showSplash(u8g2, remainingSecs);
        lastRender = now;
      }
      break;

    case Panel::MENU:
      if (panelChanged || now - lastRender >= 250UL) {
        unsigned long remainingSecs = (panelUntil > now) ? ((panelUntil - now + 999UL) / 1000UL) : 0;
        showMenu(u8g2, remainingSecs);
        lastRender = now;
      }
      break;

    case Panel::NETWORK_INFO:
      if (panelChanged || now - lastRender >= 250UL) {
        unsigned long remainingSecs = (panelUntil > now) ? ((panelUntil - now + 999UL) / 1000UL) : 0;
        showNetworkInfo(u8g2, panelData.networkSsid, panelData.networkIp, remainingSecs);
        lastRender = now;
      }
      break;

    case Panel::I2C_SCAN:
      if (panelChanged || now - lastRender >= 250UL) {
        unsigned long remainingSecs = (panelUntil > now) ? ((panelUntil - now + 999UL) / 1000UL) : 0;
        showI2CScan(u8g2, remainingSecs);
        lastRender = now;
      }
      break;

    case Panel::RTC_STATUS:
      if (panelChanged || now - lastRender >= 250UL) {
        unsigned long remainingSecs = (panelUntil > now) ? ((panelUntil - now + 999UL) / 1000UL) : 0;
        showRtcStatus(u8g2, remainingSecs);
        lastRender = now;
      }
      break;

    case Panel::ERROR_MESSAGE:
      if (panelChanged || now - lastRender >= 250UL) {
        unsigned long remainingSecs = (panelUntil > now) ? ((panelUntil - now + 999UL) / 1000UL) : 0;
        showErrorMessage(u8g2, panelData.errorMessage, remainingSecs);
        lastRender = now;
      }
      break;
  }
}

int PanelManager::panelPriority(Panel panel) {
  switch (panel) {
    case Panel::ERROR_MESSAGE:  return 20;
    case Panel::I2C_SCAN:       return 15;
    case Panel::RTC_STATUS:     return 12;
    case Panel::NETWORK_INFO:   return 10;
    case Panel::MENU:           return 5;
    case Panel::SPLASH:         return 1;
    case Panel::HISTOGRAM:      return 0;
    case Panel::GRAPH:          return 0;
    default:                    return 0;
  }
}
