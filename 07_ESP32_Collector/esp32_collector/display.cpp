#include <U8g2lib.h>

#include "config.h"
#include "display.h"

// 1.3" 128x64 I2C OLED with an SH1106 driver chip.
static U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, PIN_OLED_SCL, PIN_OLED_SDA);

// With the 6x10 font: 21 characters per line, a new line every 12 pixels.
static const int CHARS_PER_LINE = 21;
static const int LINE_HEIGHT = 12;
static const int BODY_TOP = 14;  // first line below the title and its underline

void displayInit() {
  oled.begin();
  oled.setFontPosTop();  // y in drawStr() is the top of the text, not its baseline
}

// Copies every lit pixel onto the row below it (see OLED_ROW_DOUBLE in config.h).
// The buffer is stored in 8-row "pages": one byte is 8 pixels stacked
// vertically, bit 0 at the top.
static void doubleRows() {
  uint8_t *buf = oled.getBufferPtr();
  const int width = oled.getBufferTileWidth() * 8;
  for (int page = oled.getBufferTileHeight() - 1; page >= 0; --page) {
    for (int x = 0; x < width; ++x) {
      uint8_t column = buf[page * width + x];
      // bottom pixel of the page above spills into the top pixel of this one
      uint8_t fromAbove = page > 0 ? buf[(page - 1) * width + x] >> 7 : 0;
      buf[page * width + x] = column | (column << 1) | fromAbove;
    }
  }
}

static void show() {
  if (OLED_ROW_DOUBLE) doubleRows();
  oled.sendBuffer();
}

static void begin() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
}

// Title line, cut to fit, with a line under it.
static void drawTitle(const char *title) {
  oled.drawStr(0, 0, String(title).substring(0, CHARS_PER_LINE).c_str());
  oled.drawHLine(0, 11, 128);
}

// Draws text over at most maxLines lines starting at y, breaking at spaces.
static void drawWrapped(const String &text, int y, int maxLines) {
  int start = 0;
  for (int line = 0; line < maxLines && start < (int)text.length(); ++line) {
    int end = start + CHARS_PER_LINE;
    if (end >= (int)text.length()) {
      end = text.length();
    } else {
      int space = text.lastIndexOf(' ', end);
      if (space > start) end = space;  // otherwise one long word: cut it
    }
    oled.drawStr(0, y + line * LINE_HEIGHT, text.substring(start, end).c_str());
    start = end;
    while (start < (int)text.length() && text[start] == ' ') ++start;
  }
}

void displayMessage(const char *title, const String &message) {
  begin();
  drawTitle(title);
  drawWrapped(message, BODY_TOP, 4);
  show();
}

void displayReady(int done, int total) {
  begin();
  char title[24];
  snprintf(title, sizeof title, "Ready   %d/%d done", done, total);
  drawTitle(title);
  oled.drawStr(0, BODY_TOP, "Phone WiFi:");
  oled.drawStr(0, BODY_TOP + LINE_HEIGHT, AP_SSID);
  oled.drawStr(0, BODY_TOP + 2 * LINE_HEIGHT, "Password: " AP_PASSWORD);
  oled.drawStr(0, BODY_TOP + 3 * LINE_HEIGHT, "Open 192.168.4.1");
  show();
}

void displayCountdown(const char *label, int secondsLeft) {
  begin();
  drawTitle(label);
  oled.drawStr(0, BODY_TOP, "Get ready, hold still");
  oled.setFont(u8g2_font_logisoso28_tn);
  oled.drawStr(54, 30, String(secondsLeft).c_str());
  show();
}

void displayRecording(const char *label, int secondsLeft, int heading, int scans, bool turnNow) {
  begin();
  drawTitle(label);

  char line[24];
  snprintf(line, sizeof line, "Facing %d/%d", heading + 1, HEADINGS);
  oled.drawStr(64, BODY_TOP + 2, line);
  snprintf(line, sizeof line, "Scans %d", scans);
  oled.drawStr(64, BODY_TOP + 16, line);

  if (turnNow) {
    oled.drawBox(0, 52, 128, 12);
    oled.setDrawColor(0);
    oled.drawUTF8(4, 53, "TURN RIGHT 45\xC2\xB0 NOW");
    oled.setDrawColor(1);
  } else {
    oled.drawStr(0, 53, "Stand still");
  }

  oled.setFont(u8g2_font_logisoso28_tn);
  oled.drawStr(0, BODY_TOP + 2, String(secondsLeft).c_str());
  show();
}
