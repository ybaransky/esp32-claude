#include "panel_manager.h"

#include "display_views.h"

static constexpr uint8_t       PANEL_QUEUE_CAPACITY            = 6;
static constexpr unsigned long SPLASH_DURATION_MS              = 4000;
static constexpr unsigned long DEFAULT_TIMED_PANEL_DURATION_MS = 5000;
static constexpr unsigned long TIMED_PANEL_REFRESH_MS          = 250;

struct PanelSpec {
  Panel panel;
  unsigned long durationMs;
  int priority;
};

struct PanelRequest {
  Panel         panel;
  unsigned long durationMs;
  PanelPayload  data;
};

static constexpr PanelSpec PANEL_SPECS[] = {
  {Panel::GRAPH,         0,                               0},
  {Panel::HISTOGRAM,     0,                               0},
  {Panel::SPLASH,        SPLASH_DURATION_MS,              1},
  {Panel::MENU,          DEFAULT_TIMED_PANEL_DURATION_MS, 5},
  {Panel::NETWORK_INFO,  DEFAULT_TIMED_PANEL_DURATION_MS, 10},
  {Panel::I2C_SCAN,      DEFAULT_TIMED_PANEL_DURATION_MS, 15},
  {Panel::RTC_STATUS,    DEFAULT_TIMED_PANEL_DURATION_MS, 12},
  {Panel::ERROR_MESSAGE, DEFAULT_TIMED_PANEL_DURATION_MS, 20},
};

class PanelManager {
public:
  Panel current() const { return currentPanel; }
  Panel primary() const { return primaryPanel; }
  bool hasChanged() const { return currentPanel != prevPanel; }

  void set(Panel panel, const PanelPayload &data, unsigned long now) {
    const unsigned long durationMs  = duration(panel);
    const bool          dataChanged = !(panelData == data);

    if (isPrimaryPanel(panel)) {
      primaryPanel = panel;
    }

    if (panel == currentPanel) {
      if (durationMs != 0) panelUntil = now + durationMs;
      if (dataChanged)     panelData  = data;
      return;
    }

    if (isPrimaryPanel(currentPanel)) {
      activate(panel, durationMs, data, now);
      return;
    }

    if (shouldInterruptCurrentPanel(panel)) {
      const unsigned long remainingMs = (panelUntil > now) ? (panelUntil - now) : 0;
      if (currentPanel != Panel::GRAPH) {
        enqueueFront(currentPanel, remainingMs, panelData);
      }
      activate(panel, durationMs, data, now);
      return;
    }

    enqueueBack(panel, durationMs, data);
  }

  void checkExpiration(unsigned long now) {
    if (panelUntil == 0 || now < panelUntil) return;

    PanelRequest next;
    if (dequeue(next)) {
      activate(next.panel, next.durationMs, next.data, now);
      return;
    }

    prevPanel = currentPanel;
    currentPanel = primaryPanel;
    panelUntil = 0;
    lastRender = 0;
  }

  void render(U8G2 &u8g2, unsigned long now) {
    switch (currentPanel) {
      case Panel::GRAPH:
      case Panel::HISTOGRAM:
        markRendered(now);
        break;

      case Panel::SPLASH:
        if (shouldRenderTimed(now)) {
          showSplash(u8g2, secondsRemaining(now));
          markRendered(now);
        }
        break;

      case Panel::MENU:
        if (shouldRenderTimed(now)) {
          showMenu(u8g2, secondsRemaining(now));
          markRendered(now);
        }
        break;

      case Panel::NETWORK_INFO:
        if (shouldRenderTimed(now)) {
          showNetworkInfo(u8g2, panelData.networkSsid, panelData.networkIp, secondsRemaining(now));
          markRendered(now);
        }
        break;

      case Panel::I2C_SCAN:
        if (shouldRenderTimed(now)) {
          showI2CScan(u8g2, secondsRemaining(now));
          markRendered(now);
        }
        break;

      case Panel::RTC_STATUS:
        if (shouldRenderTimed(now)) {
          showRtcStatus(u8g2, secondsRemaining(now));
          markRendered(now);
        }
        break;

      case Panel::ERROR_MESSAGE:
        if (shouldRenderTimed(now)) {
          showErrorMessage(u8g2, panelData.errorMessage, secondsRemaining(now));
          markRendered(now);
        }
        break;
    }
  }

private:
  static bool isPrimaryPanel(Panel panel) {
    return panel == Panel::GRAPH || panel == Panel::HISTOGRAM;
  }

  static const PanelSpec &specFor(Panel panel) {
    for (const PanelSpec &spec : PANEL_SPECS) {
      if (spec.panel == panel) return spec;
    }
    return PANEL_SPECS[0];
  }

  static unsigned long duration(Panel panel) {
    return specFor(panel).durationMs;
  }

  static int priority(Panel panel) {
    return specFor(panel).priority;
  }

  bool shouldInterruptCurrentPanel(Panel panel) const {
    return panel == Panel::ERROR_MESSAGE && priority(panel) > priority(currentPanel);
  }

  void activate(Panel panel, unsigned long durationMs, const PanelPayload &data, unsigned long now) {
    prevPanel    = currentPanel;
    currentPanel = panel;
    panelUntil   = (durationMs == 0) ? 0 : now + durationMs;
    panelData    = data;
    lastRender   = 0;
  }

  bool enqueueBack(Panel panel, unsigned long durationMs, const PanelPayload &data) {
    if (queuedPanelCount >= PANEL_QUEUE_CAPACITY) return false;
    queuedPanels[queuedPanelCount++] = {panel, durationMs, data};
    return true;
  }

  bool enqueueFront(Panel panel, unsigned long durationMs, const PanelPayload &data) {
    if (queuedPanelCount >= PANEL_QUEUE_CAPACITY) return false;
    for (int i = queuedPanelCount; i > 0; --i) {
      queuedPanels[i] = queuedPanels[i - 1];
    }
    queuedPanels[0] = {panel, durationMs, data};
    queuedPanelCount++;
    return true;
  }

  bool dequeue(PanelRequest &request) {
    if (queuedPanelCount == 0) return false;
    request = queuedPanels[0];
    for (uint8_t i = 1; i < queuedPanelCount; ++i) {
      queuedPanels[i - 1] = queuedPanels[i];
    }
    queuedPanelCount--;
    return true;
  }

  bool shouldRenderTimed(unsigned long now) const {
    return hasChanged() || now - lastRender >= TIMED_PANEL_REFRESH_MS;
  }

  unsigned long secondsRemaining(unsigned long now) const {
    return (panelUntil > now) ? ((panelUntil - now + 999UL) / 1000UL) : 0;
  }

  void markRendered(unsigned long now) {
    lastRender = now;
  }

  Panel         currentPanel     = Panel::GRAPH;
  Panel         prevPanel        = Panel::GRAPH;
  Panel         primaryPanel     = Panel::GRAPH;
  unsigned long panelUntil       = 0;
  unsigned long lastRender       = 0;
  PanelPayload  panelData;
  PanelRequest  queuedPanels[PANEL_QUEUE_CAPACITY] = {};
  uint8_t       queuedPanelCount = 0;
};

static PanelManager panelManager;

void panelSet(Panel panel, const PanelPayload &data, unsigned long now) {
  panelManager.set(panel, data, now);
}

void panelCheckExpiration(unsigned long now) {
  panelManager.checkExpiration(now);
}

void panelRender(U8G2 &u8g2, unsigned long now) {
  panelManager.render(u8g2, now);
}

Panel panelGetCurrent() {
  return panelManager.current();
}

Panel panelGetPrimary() {
  return panelManager.primary();
}

bool panelHasChanged() {
  return panelManager.hasChanged();
}
