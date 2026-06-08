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

struct PanelState {
  Panel         currentPanel     = Panel::GRAPH;
  Panel         prevPanel        = Panel::GRAPH;
  Panel         primaryPanel     = Panel::GRAPH;
  unsigned long panelUntil       = 0;
  unsigned long lastRender       = 0;
  PanelPayload  panelData;
  PanelRequest  queuedPanels[PANEL_QUEUE_CAPACITY];
  uint8_t       queuedPanelCount = 0;
};

static PanelState ps;

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

Panel panelGetCurrent() { return ps.currentPanel; }
Panel panelGetPrimary() { return ps.primaryPanel; }
bool  panelHasChanged() { return ps.currentPanel != ps.prevPanel; }

static const PanelSpec &panelSpec(Panel panel) {
  for (const PanelSpec &spec : PANEL_SPECS) {
    if (spec.panel == panel) return spec;
  }
  return PANEL_SPECS[0];
}

static unsigned long panelDuration(Panel panel) {
  return panelSpec(panel).durationMs;
}

static int panelPriority(Panel panel) {
  return panelSpec(panel).priority;
}

static void activatePanel(Panel panel, unsigned long durationMs, const PanelPayload &data, unsigned long now) {
  ps.prevPanel    = ps.currentPanel;
  ps.currentPanel = panel;
  ps.panelUntil   = (durationMs == 0) ? 0 : now + durationMs;
  ps.panelData    = data;
  ps.lastRender   = 0;
}

static bool enqueuePanelBack(Panel panel, unsigned long durationMs, const PanelPayload &data) {
  if (ps.queuedPanelCount >= PANEL_QUEUE_CAPACITY) return false;
  ps.queuedPanels[ps.queuedPanelCount++] = {panel, durationMs, data};
  return true;
}

static bool enqueuePanelFront(Panel panel, unsigned long durationMs, const PanelPayload &data) {
  if (ps.queuedPanelCount >= PANEL_QUEUE_CAPACITY) return false;
  for (int i = ps.queuedPanelCount; i > 0; --i) {
    ps.queuedPanels[i] = ps.queuedPanels[i - 1];
  }
  ps.queuedPanels[0] = {panel, durationMs, data};
  ps.queuedPanelCount++;
  return true;
}

static bool dequeuePanel(PanelRequest &request) {
  if (ps.queuedPanelCount == 0) return false;
  request = ps.queuedPanels[0];
  for (uint8_t i = 1; i < ps.queuedPanelCount; ++i) {
    ps.queuedPanels[i - 1] = ps.queuedPanels[i];
  }
  ps.queuedPanelCount--;
  return true;
}

void panelSet(Panel panel, const PanelPayload &data, unsigned long now) {
  const unsigned long durationMs  = panelDuration(panel);
  const bool          dataChanged = !(ps.panelData == data);

  if (panel == Panel::GRAPH || panel == Panel::HISTOGRAM) {
    ps.primaryPanel = panel;
  }

  if (panel == ps.currentPanel) {
    if (durationMs != 0) ps.panelUntil = now + durationMs;
    if (dataChanged)     ps.panelData  = data;
    return;
  }

  if (ps.currentPanel == Panel::GRAPH || ps.currentPanel == Panel::HISTOGRAM) {
    activatePanel(panel, durationMs, data, now);
    return;
  }

  if (panelPriority(panel) > panelPriority(ps.currentPanel) && panel == Panel::ERROR_MESSAGE) {
    unsigned long remainingMs = (ps.panelUntil > now) ? (ps.panelUntil - now) : 0;
    if (ps.currentPanel != Panel::GRAPH) {
      enqueuePanelFront(ps.currentPanel, remainingMs, ps.panelData);
    }
    activatePanel(panel, durationMs, data, now);
    return;
  }

  enqueuePanelBack(panel, durationMs, data);
}

void panelCheckExpiration(unsigned long now) {
  if (ps.panelUntil == 0 || now < ps.panelUntil) return;

  PanelRequest next;
  if (dequeuePanel(next)) {
    activatePanel(next.panel, next.durationMs, next.data, now);
  } else {
    ps.prevPanel    = ps.currentPanel;
    ps.currentPanel = ps.primaryPanel;
    ps.panelUntil   = 0;
    ps.lastRender   = 0;
  }
}

static bool shouldRenderTimed(unsigned long now) {
  return panelHasChanged() || now - ps.lastRender >= TIMED_PANEL_REFRESH_MS;
}

static unsigned long secondsRemaining(unsigned long now) {
  return (ps.panelUntil > now) ? ((ps.panelUntil - now + 999UL) / 1000UL) : 0;
}

static void markRendered(unsigned long now) {
  ps.lastRender = now;
}

void panelRender(U8G2 &u8g2, unsigned long now) {
  switch (ps.currentPanel) {
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
        showNetworkInfo(u8g2, ps.panelData.networkSsid, ps.panelData.networkIp, secondsRemaining(now));
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
        showErrorMessage(u8g2, ps.panelData.errorMessage, secondsRemaining(now));
        markRendered(now);
      }
      break;
  }
}
