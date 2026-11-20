// Урок 3 — Make it show & listen 👀
// Arduino Uno + DFPlayer Mini (урок 1) + DS3231 (урок 2) + OLED SSD1306 + дві кнопки.
// Мета: пристрій ПОКАЗУЄ час і розклад, а розклад можна змінити КНОПКАМИ,
// без комп'ютера і без перепрошивки.
//
// Схема (див. ../README.md та ../wokwi/diagram.json):
//   DFPlayer VCC  → Uno 5V           DS3231 VCC → Uno 5V
//   DFPlayer GND  → Uno GND          DS3231 GND → Uno GND
//   DFPlayer TX   → Uno D10          DS3231 SDA → Uno A4 (SDA)
//   DFPlayer RX   ← 1 кОм ← Uno D11  DS3231 SCL → Uno A5 (SCL)
//   DFPlayer BUSY → Uno D7  (LOW = трек грає)
//   DFPlayer SPK_1/SPK_2 → динамік 4–8 Ом, до 3 Вт
//   OLED SDA → A4, SCL → A5 (та сама шина I²C, що й годинник), адреса 0x3C
//   OLED VCC / GND — живлення за написами на вашому модулі (перевірити!)
//   Кнопка A «+»  : один вивід → D2, другий → GND
//   Кнопка B «→»  : один вивід → D3, другий → GND
//   Резистори для кнопок не потрібні: вмикаємо внутрішню підтяжку INPUT_PULLUP.
//
// Бібліотеки (Arduino Library Manager): DFRobotDFPlayerMini, RTClib (Adafruit), U8g2.
// Файл icons.h лежить поруч: у ньому іконки екрана, намальовані прямо в коментарях.
// З U8g2 беремо текстовий режим U8x8: він малює прямо в дисплей і НЕ тримає
// копію екрана в пам'яті (Adafruit SSD1306 забрав би 1 КБ з 2 КБ RAM Uno).
//
// Кнопки:
//   A + B разом (1-й раз) — меню 1/3: поточний час (записується в годинник DS3231)
//   A + B разом (2-й раз) — зберегти, меню 2/3: час хвилини мовчання (гімн — на хвилину пізніше)
//   A + B разом (3-й раз) — зберегти, меню 3/3: час цитати дня
//   A + B разом (4-й раз) — зберегти і повернутися на головний екран
//   Час живе в DS3231 (батарейка тримає його без живлення), розклад — в EEPROM.
//   У меню: A — +1 (утримання — швидко), B — перейти між годинами й хвилинами.
//   30 с без натискань — вихід з меню без збереження поточного пункту.
//
// Команди в Serial Monitor (9600 бод, «Новий рядок») — як в уроці 2:
//   HELP | NOW | SET 2026-10-16 08:59:55 | SCHEDULE | PLAY [трек] | STOP | CLOCK ON/OFF
//
// Правила чесності (з уроків 1–2):
//   * «ГРАЄ» і значок ▶ на екрані показуємо лише після підтвердження BUSY = LOW.
//   * Без годинника або з невстановленим часом нічого не запускаємо.
//   * Кожна подія спрацьовує один раз на день.
//   * Настала наступна подія, а щось ще грає → старий трек зупиняємо, грає нова подія.
//   * Цитату не можна поставити на час хвилини мовчання чи гімну — меню не збереже.

#include <Wire.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <RTClib.h>
#include <DFRobotDFPlayerMini.h>
#include <U8x8lib.h>
#include "icons.h"  // малюнки 16×16 для екрана

// --- Піни (Arduino Uno) ---
const uint8_t PIN_MP3_RX = 10;   // Uno приймає ← TX DFPlayer
const uint8_t PIN_MP3_TX = 11;   // Uno передає → 1 кОм → RX DFPlayer
const uint8_t PIN_MP3_BUSY = 7;  // LOW, поки трек грає
const uint8_t PIN_BTN_PLUS = 2;  // кнопка A «+»
const uint8_t PIN_BTN_NEXT = 3;  // кнопка B «→»
// Годинник і дисплей сидять на одній шині I²C (A4 = SDA, A5 = SCL), але мають різні адреси.
const uint8_t RTC_I2C_ADDRESS = 0x68;
const uint8_t OLED_I2C_ADDRESS = 0x3C;  // у деяких модулів 0x3D — дивіться напис на платі

// --- Налаштування ---
const uint8_t VOLUME = 20;                           // 0..30
const unsigned long PLAY_CONFIRM_TIMEOUT_MS = 1500;  // скільки чекаємо BUSY = LOW
const unsigned long CLOCK_CHECK_MS = 1000;           // як часто читаємо годинник
const uint16_t MIN_VALID_YEAR = 2025;                // раніше — час точно не встановлено
const unsigned long DEBOUNCE_MS = 30;                // антибрязкіт кнопок
const unsigned long REPEAT_DELAY_MS = 600;           // утримуєш «+» довше — починається повтор
const unsigned long REPEAT_EVERY_MS = 150;           // швидкість повтору
const unsigned long MENU_TIMEOUT_MS = 30000;         // меню закривається само
const unsigned long NOTICE_MS = 4000;                // скільки висить коротке повідомлення
const unsigned long LOGO_MS = 2000;                  // скільки показуємо логотип під час запуску

// --- Розклад: три події, у кожної — своя група файлів на SD ---
// DFPlayer рахує файли в ПОРЯДКУ КОПІЮВАННЯ на SD, тож копіюйте саме так:
//   0001–0003.mp3 — гімн України (3 варіанти)
//   0004–0006.mp3 — хвилина мовчання (3 варіанти)
//   0007.mp3 і далі — цитати дня (скільки є файлів)
// Для кожної події трек обирається випадково зі своєї групи
// і не повторюється двічі поспіль.
enum EventId { EV_SILENCE, EV_ANTHEM, EV_QUOTE, EVENT_COUNT };
const uint8_t GROUP_FIRST[EVENT_COUNT] = { 4, 1, 7 };  // мовчання, гімн, цитата
const uint8_t GROUP_LAST[EVENT_COUNT] = { 6, 3, 0 };   // 0 — до останнього файлу на SD
const unsigned long STOP_WAIT_MS = 1000;               // скільки чекаємо, поки старий трек замовкне

// Розклад «з коробки»: записується в EEPROM, якщо там ще нічого немає.
const uint8_t DEFAULT_TIME[EVENT_COUNT][2] = {
  { 9,  0},  // 09:00 — хвилина мовчання
  { 9,  1},  // 09:01 — гімн (завжди на хвилину після хвилини мовчання)
  {12, 10},  // 12:10 — цитата дня, велика перерва
};

// --- EEPROM: розклад і дати останніх запусків переживають перезапуск ---
const int EEPROM_ADDR = 0;
const uint8_t EEPROM_MAGIC = 0xC3;  // формат уроку 3 (в уроці 2 був 0xB3)

struct ScheduleEvent {
  uint8_t hour;
  uint8_t minute;
  uint32_t lastFiredDay;  // РРРРММДД дня, коли подія вже спрацювала (0 = ще ні)
};

struct Settings {
  uint8_t magic;
  ScheduleEvent events[EVENT_COUNT];
};

// Кнопка з антибрязкотом (див. розділ «Кнопки» нижче)
struct Button {
  uint8_t pin;
  bool pressed;               // стабільний стан після антибрязкоту
  bool rawPressed;            // що показує пін просто зараз
  unsigned long rawChangedAt;
};

SoftwareSerial mp3Serial(PIN_MP3_RX, PIN_MP3_TX);
DFRobotDFPlayerMini player;
RTC_DS3231 rtc;
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);  // 16 × 8 клітинок по 8×8 пікселів

// Стани плеєра (як в уроці 1)
enum PlayerState {
  STATE_NO_PLAYER,  // модуль не відповів або немає SD-карти
  STATE_READY,      // готовий, нічого не грає
  STATE_STARTING,   // команду play надіслано, чекаємо підтвердження
  STATE_PLAYING,    // BUSY = LOW, звук підтверджено
  STATE_STOPPING    // зупиняємо старий трек, щоб почати наступну подію
};

// Стани годинника (як в уроці 2)
enum ClockState {
  CLOCK_MISSING,  // DS3231 не відповідає на шині I²C
  CLOCK_NOT_SET,  // відповідає, але час не встановлено / збився
  CLOCK_OK        // час можна використовувати для розкладу
};

// Що зараз на екрані
enum UiMode {
  UI_HOME,         // годинник і розклад
  UI_SET_CLOCK,    // меню 1/3: поточний час
  UI_SET_SILENCE,  // меню 2/3
  UI_SET_QUOTE     // меню 3/3
};

PlayerState playerState = STATE_NO_PLAYER;
ClockState clockState = CLOCK_MISSING;
UiMode uiMode = UI_HOME;
Settings settings;

int fileCount = -1;        // скільки файлів на SD (-1 = невідомо)
uint16_t pendingTrack = 0;     // трек, що стартує, щойно замовкне старий (0 = немає)
int8_t pendingEventIdx = -1;   // до якої події він належить
int8_t currentEvent = -1;      // яка подія зараз грає (-1 = ручний PLAY)
uint16_t lastPicked[EVENT_COUNT] = { 0, 0, 0 };  // щоб не повторювати трек двічі поспіль
unsigned long stopRequestedAt = 0;
unsigned long startRequestedAt = 0;
unsigned long lastClockCheckAt = 0;
bool printEverySecond = true;

DateTime lastNow;          // останній прочитаний час
bool clockReady = false;   // чи можна йому довіряти

char commandLine[40];
uint8_t commandLength = 0;

// ---------------------------------------------------------------------------
// Допоміжні функції друку

void print2(uint8_t value) {
  if (value < 10) Serial.print('0');
  Serial.print(value);
}

void printDateTime(const DateTime &t) {
  Serial.print(t.year());
  Serial.print('-');
  print2(t.month());
  Serial.print('-');
  print2(t.day());
  Serial.print(' ');
  print2(t.hour());
  Serial.print(':');
  print2(t.minute());
  Serial.print(':');
  print2(t.second());
}

const __FlashStringHelper *eventName(uint8_t i) {
  switch (i) {
    case EV_SILENCE: return F("хвилина мовчання");
    case EV_ANTHEM:  return F("гімн України");
    default:         return F("цитата дня");
  }
}

void printEvent(uint8_t i) {
  const ScheduleEvent &e = settings.events[i];
  Serial.print(i + 1);
  Serial.print(F(") "));
  print2(e.hour);
  Serial.print(':');
  print2(e.minute);
  Serial.print(F(" → треки "));
  Serial.print(GROUP_FIRST[i]);
  if (GROUP_LAST[i] == 0) {
    Serial.print('+');
  } else {
    Serial.print('-');
    Serial.print(GROUP_LAST[i]);
  }
  Serial.print(F(" ("));
  Serial.print(eventName(i));
  Serial.print(')');
}

void printSchedule() {
  Serial.println(F("Розклад (EEPROM):"));
  for (uint8_t i = 0; i < EVENT_COUNT; i++) {
    Serial.print(F("  "));
    printEvent(i);
    if (settings.events[i].lastFiredDay != 0) {
      Serial.print(F(" | останній запуск: "));
      Serial.print(settings.events[i].lastFiredDay);
    }
    Serial.println();
  }
}

void printHelp() {
  Serial.println(F("Кнопки: A+B — меню, A — +1, B — години/хвилини."));
  Serial.println(F("Команди: NOW | SET РРРР-ММ-ДД ГГ:ХХ:СС | SCHEDULE"));
  Serial.println(F("         PLAY [трек] | STOP | CLOCK ON | CLOCK OFF | HELP"));
}

// ---------------------------------------------------------------------------
// EEPROM і розклад

void saveSettings() {
  EEPROM.put(EEPROM_ADDR, settings);  // put() перезаписує лише змінені байти
}

bool settingsLookValid() {
  if (settings.magic != EEPROM_MAGIC) return false;
  for (uint8_t i = 0; i < EVENT_COUNT; i++) {
    if (settings.events[i].hour > 23 || settings.events[i].minute > 59) return false;
  }
  return true;
}

// Читає розклад з EEPROM. Якщо там порожньо або старий формат уроку 2 —
// записує розклад «з коробки». Код більше НЕ перезаписує розклад при кожному
// старті: тепер його змінюють кнопки, і зміни мають пережити RESET.
void loadSettings() {
  EEPROM.get(EEPROM_ADDR, settings);
  if (settingsLookValid()) return;

  settings.magic = EEPROM_MAGIC;
  for (uint8_t i = 0; i < EVENT_COUNT; i++) {
    settings.events[i].hour = DEFAULT_TIME[i][0];
    settings.events[i].minute = DEFAULT_TIME[i][1];
    settings.events[i].lastFiredDay = 0;
  }
  saveSettings();
  Serial.println(F("EEPROM порожня або старого формату — записано розклад за замовчуванням."));
}

// Змінює час події і зберігає в EEPROM. Нова година — нова подія,
// тож «вже грала сьогодні» скидаємо. Повертає true, якщо щось змінилось.
bool setEventTime(uint8_t i, uint8_t hour, uint8_t minute) {
  ScheduleEvent &e = settings.events[i];
  if (e.hour == hour && e.minute == minute) return false;
  e.hour = hour;
  e.minute = minute;
  e.lastFiredDay = 0;
  saveSettings();
  Serial.print(F("Збережено: "));
  printEvent(i);
  Serial.println();
  return true;
}

// Хвилина мовчання + гімн через хвилину (через північ теж працює: 23:59 → 00:00).
void setSilenceTime(uint8_t hour, uint8_t minute) {
  setEventTime(EV_SILENCE, hour, minute);
  uint16_t anthem = (hour * 60 + minute + 1) % 1440;
  setEventTime(EV_ANTHEM, anthem / 60, anthem % 60);
}

uint32_t dayKey(const DateTime &t) {
  return (uint32_t)t.year() * 10000UL + t.month() * 100UL + t.day();
}

// ---------------------------------------------------------------------------
// Годинник DS3231 (без змін з уроку 2)

bool i2cResponds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool timeLooksValid(const DateTime &t) {
  return t.isValid() && t.year() >= MIN_VALID_YEAR && t.year() <= 2099;
}

void printClockState(ClockState state) {
  switch (state) {
    case CLOCK_MISSING:
      Serial.println(F("ПОМИЛКА: годинник DS3231 не відповідає. Перевірте SDA→A4, SCL→A5, 5V, GND."));
      Serial.println(F("Розклад призупинено: без часу нічого не запускаємо."));
      break;
    case CLOCK_NOT_SET:
      Serial.println(F("УВАГА: час не встановлено або збився (батарейка?)."));
      Serial.println(F("Встановіть час: SET 2026-10-16 08:59:50"));
      break;
    case CLOCK_OK:
      Serial.println(F("Годинник: час встановлено і читається."));
      break;
  }
}

void setClockState(ClockState newState) {
  if (newState == clockState) return;
  clockState = newState;
  printClockState(newState);
}

// Перевіряє, чи годинник на місці і чи час правдоподібний.
// Повертає true і заповнює now, якщо часу можна довіряти.
bool readClock(DateTime &now) {
  if (!i2cResponds(RTC_I2C_ADDRESS)) {
    setClockState(CLOCK_MISSING);
    return false;
  }
  if (clockState == CLOCK_MISSING) {
    // Годинник щойно з'явився: (пере)ініціалізуємо бібліотеку.
    if (!rtc.begin()) {
      return false;
    }
    if (rtc.lostPower()) {
      setClockState(CLOCK_NOT_SET);
      return false;
    }
  }
  now = rtc.now();
  if (!timeLooksValid(now)) {
    setClockState(CLOCK_NOT_SET);
    return false;
  }
  if (clockState == CLOCK_NOT_SET && rtc.lostPower()) {
    return false;  // чекаємо на команду SET
  }
  setClockState(CLOCK_OK);
  return true;
}

// Скільки секунд від now до події i (якщо сьогодні вже минула — до завтрашньої).
long secondsUntil(const DateTime &now, uint8_t i) {
  long nowSec = now.hour() * 3600L + now.minute() * 60L + now.second();
  long diff = settings.events[i].hour * 3600L + settings.events[i].minute * 60L - nowSec;
  if (diff < 0) diff += 86400L;
  return diff;
}

void printClockLine(const DateTime &now) {
  Serial.print(F("Час: "));
  printDateTime(now);
  int8_t next = -1;
  long best = 0;
  for (uint8_t i = 0; i < EVENT_COUNT; i++) {
    long left = secondsUntil(now, i);
    if (next < 0 || left < best) {
      best = left;
      next = i;
    }
  }
  Serial.print(F(" | до події "));
  Serial.print(next + 1);
  Serial.print(F(": "));
  print2(best / 3600);
  Serial.print(':');
  print2((best / 60) % 60);
  Serial.print(':');
  print2(best % 60);
  if (playerState == STATE_PLAYING) Serial.print(F(" | ГРАЄ"));
  Serial.println();
}

// ---------------------------------------------------------------------------
// Дисплей OLED SSD1306, 128×64 → 16 стовпців × 8 рядків клітинок по 8×8 пікселів.
// Екран без слів — 4 лінії по 2 рядки клітинок. Кожна лінія: іконка 16×16 (icons.h),
// час великими цифрами 2×2 і позначка праворуч:
//
//   [годинник]  09:41  [стан: ⚠ / 🔇 / ✓ або порожньо]
//   [свічка]    09:00  [▶ — грає]
//   [прапор]    09:01
//   [хмаринка]  12:10
//
// Малюємо лише лінії, що змінились: менше даних по I²C, екран не блимає.

bool displayOk = false;
bool screenDirty = true;            // треба перемалювати
char lineCache[4][12];              // що зараз на кожній лінії
uint8_t noticeIcon = ICON_NONE;     // коротка позначка біля годинника (✓, ⚠, 🔇)
unsigned long noticeAt = 0;
const uint8_t EVENT_ICON[EVENT_COUNT] = { ICON_CANDLE, ICON_FLAG, ICON_QUOTE };

void resetScreen() {
  memset(lineCache, 0, sizeof(lineCache));
  if (displayOk) oled.clearDisplay();
  screenDirty = true;
}

// Позначка на кілька секунд праворуч від годинника.
void showNotice(uint8_t icon) {
  noticeIcon = icon;
  noticeAt = millis();
  screenDirty = true;
}

// Іконка 16×16 = 2×2 клітинки. Копіюємо з флеш-пам'яті по половинці (16 байт).
void drawIcon(uint8_t col, uint8_t row, uint8_t icon) {
  uint8_t tiles[16];
  memcpy_P(tiles, ICONS[icon], 16);
  oled.drawTile(col, row, 2, tiles);
  memcpy_P(tiles, ICONS[icon] + 16, 16);
  oled.drawTile(col, row + 1, 2, tiles);
}

// Лінія 0..3: іконка, час «ГГ:ХХ» (5 знаків) і позначка.
// editField: -1 — звичайно, 0 — години інверсні, 1 — хвилини інверсні (меню).
void showLine(uint8_t line, uint8_t icon, const char *time, uint8_t mark, int8_t editField) {
  char key[12];
  snprintf_P(key, sizeof(key), PSTR("%u%s%u%d"), icon, time, mark, editField);
  if (strcmp(key, lineCache[line]) == 0) return;
  strcpy(lineCache[line], key);

  uint8_t row = line * 2;
  drawIcon(0, row, icon);
  oled.setFont(u8x8_font_px437wyse700b_2x2_n);
  char part[3] = { time[0], time[1], '\0' };
  oled.setInverseFont(editField == 0);
  oled.drawString(3, row, part);           // години: стовпці 3–6
  oled.setInverseFont(0);
  part[0] = time[2];
  part[1] = '\0';
  oled.drawString(7, row, part);           // двокрапка: 7–8
  part[0] = time[3];
  part[1] = time[4];
  oled.setInverseFont(editField == 1);
  oled.drawString(9, row, part);           // хвилини: 9–12
  oled.setInverseFont(0);
  drawIcon(14, row, mark);
}

// Позначка біля годинника. Порожньо — усе гаразд.
uint8_t statusIcon() {
  if (noticeIcon != ICON_NONE) return noticeIcon;
  if (!clockReady) return ICON_WARN;              // годинника немає або час не встановлено
  if (playerState == STATE_NO_PLAYER) return ICON_MUTE;
  return ICON_NONE;
}

void renderHome() {
  char t[6];
  if (clockReady) {
    // Двокрапка блимає раз на секунду — видно, що годинник іде.
    sprintf_P(t, PSTR("%02u%c%02u"), lastNow.hour(), lastNow.second() % 2 ? ' ' : ':', lastNow.minute());
  } else {
    strcpy_P(t, PSTR("--:--"));  // без годинника не вигадуємо час
  }
  showLine(0, ICON_CLOCK, t, statusIcon(), -1);

  for (uint8_t i = 0; i < EVENT_COUNT; i++) {
    sprintf_P(t, PSTR("%02u:%02u"), settings.events[i].hour, settings.events[i].minute);
    // ▶ — лише коли BUSY підтвердив, що грає саме цей трек.
    bool playing = playerState == STATE_PLAYING && currentEvent == (int8_t)i;
    showLine(i + 1, EVENT_ICON[i], t, playing ? ICON_PLAY : ICON_NONE, -1);
  }
}

uint8_t editHour = 0;
uint8_t editMinute = 0;
bool editingMinutes = false;  // false — міняємо години, true — хвилини
bool editChanged = false;     // чи натискали «+» у цьому пункті меню
unsigned long lastUiActionAt = 0;

void renderMenu() {
  bool silence = uiMode == UI_SET_SILENCE;
  uint8_t step = uiMode == UI_SET_CLOCK ? 1 : silence ? 2 : 3;
  char t[6];
  sprintf_P(t, PSTR(" %u/3 "), step);
  showLine(0, ICON_PENCIL, t, noticeIcon, -1);  // ⚠, якщо час не можна зберегти

  sprintf_P(t, PSTR("%02u:%02u"), editHour, editMinute);
  uint8_t icon = step == 1 ? ICON_CLOCK : silence ? ICON_CANDLE : ICON_QUOTE;
  showLine(1, icon, t, ICON_NONE, editingMinutes ? 1 : 0);

  if (silence) {
    uint16_t anthem = (editHour * 60 + editMinute + 1) % 1440;  // гімн — на хвилину пізніше
    sprintf_P(t, PSTR("%02u:%02u"), anthem / 60, anthem % 60);
    showLine(2, ICON_FLAG, t, ICON_NONE, -1);
  } else {
    showLine(2, ICON_NONE, "     ", ICON_NONE, -1);
  }

  // Підказка дрібним шрифтом у нижньому рядку (малюємо один раз за вхід у меню).
  if (lineCache[3][0] != 'h') {
    strcpy(lineCache[3], "h");
    oled.setFont(u8x8_font_chroma48medium8_r);
    oled.drawString(0, 7, "A:+  B:<>  AB:OK");
  }
}

// Заставка під час запуску:
//
//   [дзвоник]  STEM  [дзвоник]
//   ────────────────────────
//              93
//
//          SCHOOL BELL
void showLogo() {
  oled.clearDisplay();
  drawIcon(1, 1, ICON_BELL);
  drawIcon(13, 1, ICON_BELL);
  // Без нових шрифтів (флеш Uno майже повна): звичайні літери, збільшені вдвічі,
  // і цифри тим самим шрифтом 2×2, що й час.
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.draw2x2String(4, 1, "STEM");
  uint8_t line[8] = { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18 };  // риска посередині клітинки
  for (uint8_t col = 1; col < 15; col++) oled.drawTile(col, 3, 1, line);
  oled.setFont(u8x8_font_px437wyse700b_2x2_n);
  oled.drawString(6, 4, "93");
  oled.setFont(u8x8_font_chroma48medium8_r);
  oled.drawString(2, 7, "SCHOOL  BELL");
}

void renderScreen() {
  if (!displayOk) return;
  if (uiMode == UI_HOME) renderHome();
  else renderMenu();
}

// Шукає дисплей на шині. Якщо з'явився — вмикаємо, зник — пишемо помилку.
void checkDisplay() {
  bool present = i2cResponds(OLED_I2C_ADDRESS);
  if (present == displayOk) return;
  displayOk = present;
  if (present) {
    oled.begin();
    oled.setPowerSave(0);
    resetScreen();
    Serial.println(F("Дисплей: OLED відповідає на 0x3C."));
  } else {
    Serial.println(F("ПОМИЛКА: OLED не відповідає на 0x3C. Перевірте SDA→A4, SCL→A5, VCC, GND."));
  }
}

// ---------------------------------------------------------------------------
// DFPlayer (логіка з уроку 1)

bool isBusyLow() {
  return digitalRead(PIN_MP3_BUSY) == LOW;
}

// Випадковий трек із групи події i, не той самий, що минулого разу.
// Повертає 0, якщо файлів цієї групи на SD немає.
uint16_t pickTrack(uint8_t i) {
  uint16_t first = GROUP_FIRST[i];
  uint16_t last = GROUP_LAST[i];
  if (fileCount >= 0) {
    if (fileCount < (int)first) return 0;
    if (last == 0 || fileCount < (int)last) last = fileCount;
  } else if (last == 0) {
    last = first;  // скільки цитат — невідомо: граємо першу
  }
  uint16_t count = last - first + 1;
  // Підмішуємо час: подія щодня в ту саму хвилину, але дата щодня інша.
  randomSeed(random(2147483647L) ^ lastNow.unixtime() ^ micros());
  uint16_t track = first + random(count);
  if (count > 1 && track == lastPicked[i]) {
    track = first + (track - first + 1 + random(count - 1)) % count;
  }
  lastPicked[i] = track;
  return track;
}

void startTrackFor(uint16_t track, int8_t eventIdx) {
  if (playerState == STATE_NO_PLAYER) {
    Serial.println(F("ПОМИЛКА: DFPlayer недоступний — відтворення неможливе."));
    showNotice(ICON_MUTE);
    return;
  }
  if (fileCount >= 0 && fileCount < (int)track) {
    Serial.print(F("ПОМИЛКА: на SD-карті немає треку "));
    Serial.println(track);
    showNotice(ICON_WARN);
    return;
  }
  Serial.print(F("Команда: відтворити трек "));
  Serial.println(track);
  player.play(track);
  currentEvent = eventIdx;
  startRequestedAt = millis();
  playerState = STATE_STARTING;
  screenDirty = true;
}

// Ручний запуск (команда PLAY) — не належить жодній події.
void startTrack(uint16_t track) {
  startTrackFor(track, -1);
}

void printPlayerMessage(uint8_t type, int value) {
  switch (type) {
    case DFPlayerPlayFinished:
      Serial.print(F("DFPlayer: трек завершився: "));
      Serial.println(value);
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("DFPlayer: SD-карту вийнято"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("DFPlayer: SD-карту вставлено"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayer: помилка "));
      Serial.print(value);
      switch (value) {
        case Busy:             Serial.println(F(" (модуль не готовий / немає носія)")); break;
        case FileIndexOut:     Serial.println(F(" (немає треку з таким номером)")); break;
        case FileMismatch:     Serial.println(F(" (файл не знайдено)")); break;
        case CheckSumNotMatch: Serial.println(F(" (помилка контрольної суми)")); break;
        default:               Serial.println(); break;
      }
      break;
    default:
      break;
  }
}

void updatePlayer() {
  if (playerState != STATE_NO_PLAYER && player.available()) {
    printPlayerMessage(player.readType(), player.read());
  }

  switch (playerState) {
    case STATE_STARTING:
      if (isBusyLow()) {
        playerState = STATE_PLAYING;
        screenDirty = true;
        Serial.println(F("ГРАЄ: відтворення підтверджено (BUSY = LOW)."));
      } else if (millis() - startRequestedAt >= PLAY_CONFIRM_TIMEOUT_MS) {
        playerState = STATE_READY;
        showNotice(ICON_WARN);
        Serial.println(F("ПОМИЛКА: трек не запустився (немає підтвердження BUSY)."));
      }
      break;

    case STATE_PLAYING:
      if (!isBusyLow()) {
        playerState = STATE_READY;
        screenDirty = true;
        Serial.println(F("ГОТОВО: трек більше не грає."));
      }
      break;

    case STATE_STOPPING:
      // Старий трек замовк (BUSY = HIGH) або чекали досить — запускаємо нову подію.
      if (!isBusyLow() || millis() - stopRequestedAt >= STOP_WAIT_MS) {
        playerState = STATE_READY;
        if (pendingTrack != 0) {
          uint16_t track = pendingTrack;
          pendingTrack = 0;
          startTrackFor(track, pendingEventIdx);
        }
      }
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Розклад: кожна подія — один раз на день (з уроку 2)

bool playerBusy() {
  return playerState == STATE_STARTING || playerState == STATE_PLAYING;
}

// Запускає випадковий трек події i. Якщо щось ще грає — спершу зупиняє його.
void playEvent(uint8_t i) {
  uint16_t track = pickTrack(i);
  if (track == 0) {
    Serial.print(F("ПОМИЛКА: на SD немає файлів для події «"));
    Serial.print(eventName(i));
    Serial.println(F("»."));
    showNotice(ICON_WARN);
    return;
  }
  if (playerState == STATE_STOPPING) {  // уже зупиняємо — просто міняємо, що грати далі
    pendingTrack = track;
    pendingEventIdx = i;
    return;
  }
  if (playerBusy()) {
    Serial.println(F("Зупиняємо попередній трек: настав час наступної події."));
    player.stop();
    pendingTrack = track;
    pendingEventIdx = i;
    stopRequestedAt = millis();
    playerState = STATE_STOPPING;
    screenDirty = true;
    return;
  }
  startTrackFor(track, i);
}

void checkSchedule(const DateTime &now) {
  uint32_t today = dayKey(now);
  for (uint8_t i = 0; i < EVENT_COUNT; i++) {
    ScheduleEvent &e = settings.events[i];
    if (now.hour() != e.hour || now.minute() != e.minute) continue;
    if (e.lastFiredDay == today) continue;  // сьогодні вже спрацювала

    // Запам'ятовуємо ДО запуску: навіть якщо трек не стартує,
    // не смикаємо плеєр щосекунди протягом усієї хвилини.
    e.lastFiredDay = today;
    saveSettings();

    Serial.print(F("⏰ НАСТАВ ЧАС ПОДІЇ "));
    printEvent(i);
    Serial.println();

    playEvent(i);
  }
}

// ---------------------------------------------------------------------------
// Меню

void printEditValue() {
  Serial.print(F("  меню: "));
  print2(editHour);
  Serial.print(':');
  print2(editMinute);
  Serial.println(editingMinutes ? F(" (змінюємо хвилини)") : F(" (змінюємо години)"));
}

void enterMenu(UiMode mode) {
  uiMode = mode;
  if (mode == UI_SET_CLOCK) {
    // Починаємо з того, що показує годинник; якщо часу немає — з 12:00.
    editHour = clockReady ? lastNow.hour() : 12;
    editMinute = clockReady ? lastNow.minute() : 0;
    Serial.println(F("Меню 1/3: поточний час"));
  } else {
    const ScheduleEvent &e = settings.events[mode == UI_SET_SILENCE ? EV_SILENCE : EV_QUOTE];
    editHour = e.hour;
    editMinute = e.minute;
    Serial.println(mode == UI_SET_SILENCE ? F("Меню 2/3: час хвилини мовчання")
                                          : F("Меню 3/3: час цитати дня"));
  }
  editingMinutes = false;
  editChanged = false;
  printEditValue();
  resetScreen();
}

// Записує години й хвилини в DS3231 (секунди = 0). Дату залишаємо ту, що в годиннику;
// якщо годинник ще не знав дати — ставимо 2026-01-01 (точну дату задає команда SET).
// Повертає true, якщо час записано і він читається.
bool saveClockTime(uint8_t hour, uint8_t minute) {
  if (!i2cResponds(RTC_I2C_ADDRESS)) {
    setClockState(CLOCK_MISSING);
    Serial.println(F("ПОМИЛКА: годинник не відповідає, час не записано."));
    return false;
  }
  if (clockState == CLOCK_MISSING) rtc.begin();
  DateTime base = rtc.now();
  if (!timeLooksValid(base)) base = DateTime(2026, 1, 1, 0, 0, 0);  // дату можна виправити командою SET
  DateTime t(base.year(), base.month(), base.day(), hour, minute, 0);
  rtc.adjust(t);
  Serial.print(F("Годинник встановлено: "));
  printDateTime(t);
  Serial.println();
  clockState = CLOCK_NOT_SET;  // перевіримо, що час справді читається
  clockReady = readClock(lastNow);
  return clockReady;
}

void leaveMenu() {
  uiMode = UI_HOME;
  resetScreen();
}

// Цитата не може звучати одночасно з хвилиною мовчання або гімном
// (гімн — через хвилину після мовчання). Такий розклад не зберігаємо.
bool quoteClashes(uint8_t silenceHour, uint8_t silenceMinute, uint8_t quoteHour, uint8_t quoteMinute) {
  uint16_t silence = silenceHour * 60 + silenceMinute;
  uint16_t anthem = (silence + 1) % 1440;
  uint16_t quote = quoteHour * 60 + quoteMinute;
  return quote == silence || quote == anthem;
}

void rejectClash() {
  Serial.println(F("ПОМИЛКА: цитата не може збігатися з хвилиною мовчання чи гімном. Оберіть інший час."));
  showNotice(ICON_WARN);
}

// A + B разом: наступний крок меню (зі збереженням поточного).
void onBothButtons() {
  lastUiActionAt = millis();
  switch (uiMode) {
    case UI_HOME:
      enterMenu(UI_SET_CLOCK);
      break;
    case UI_SET_CLOCK:
      // Не чіпали «+» і час уже правильний — не перезаписуємо (інакше обнулимо секунди).
      if (editChanged || !clockReady) {
        if (!saveClockTime(editHour, editMinute)) showNotice(ICON_WARN);
      } else {
        Serial.println(F("Час не змінено."));
      }
      enterMenu(UI_SET_SILENCE);
      break;
    case UI_SET_SILENCE:
      if (quoteClashes(editHour, editMinute, settings.events[EV_QUOTE].hour, settings.events[EV_QUOTE].minute)) {
        rejectClash();  // лишаємось у цьому меню
        break;
      }
      setSilenceTime(editHour, editMinute);
      enterMenu(UI_SET_QUOTE);
      break;
    case UI_SET_QUOTE:
      if (quoteClashes(settings.events[EV_SILENCE].hour, settings.events[EV_SILENCE].minute, editHour, editMinute)) {
        rejectClash();
        break;
      }
      setEventTime(EV_QUOTE, editHour, editMinute);
      Serial.println(F("Меню закрито, розклад збережено в EEPROM."));
      printSchedule();
      leaveMenu();
      showNotice(ICON_CHECK);
      break;
  }
}

// A: +1 до години або хвилини (по колу: 23 → 0, 59 → 0).
void onPlus() {
  if (uiMode == UI_HOME) return;
  lastUiActionAt = millis();
  editChanged = true;
  if (editingMinutes) editMinute = (editMinute + 1) % 60;
  else editHour = (editHour + 1) % 24;
  printEditValue();
  screenDirty = true;
}

// B: перемкнутися між годинами й хвилинами.
void onNext() {
  if (uiMode == UI_HOME) return;
  lastUiActionAt = millis();
  editingMinutes = !editingMinutes;
  printEditValue();
  screenDirty = true;
}

// ---------------------------------------------------------------------------
// Кнопки: антибрязкіт + розпізнавання «A», «B» і «A+B разом»

Button btnPlus = { PIN_BTN_PLUS, false, false, 0 };
Button btnNext = { PIN_BTN_NEXT, false, false, 0 };

// Повертає true, якщо стабільний стан щойно змінився.
bool updateButton(Button &b) {
  bool raw = digitalRead(b.pin) == LOW;  // INPUT_PULLUP: натиснуто = LOW
  if (raw != b.rawPressed) {
    b.rawPressed = raw;  // контакт «брязкає» — чекаємо, поки заспокоїться
    b.rawChangedAt = millis();
    return false;
  }
  if (raw != b.pressed && millis() - b.rawChangedAt >= DEBOUNCE_MS) {
    b.pressed = raw;
    return true;
  }
  return false;
}

// «Жест» триває від першого натискання до моменту, коли відпущені обидві кнопки.
// Одиночну дію виконуємо на ВІДПУСКАННЯ: так людина встигає дотиснути другу
// кнопку, і «A+B» не перетворюється випадково на «A».
bool gestureActive = false;
bool gesturePlus = false;
bool gestureNext = false;
bool gestureBoth = false;
bool plusRepeated = false;
unsigned long plusPressedAt = 0;
unsigned long lastRepeatAt = 0;

void readButtons() {
  bool plusChanged = updateButton(btnPlus);
  updateButton(btnNext);

  if (btnPlus.pressed || btnNext.pressed) {
    if (!gestureActive) {
      gestureActive = true;
      gesturePlus = gestureNext = gestureBoth = plusRepeated = false;
    }
    if (btnPlus.pressed) gesturePlus = true;
    if (btnNext.pressed) gestureNext = true;
    if (plusChanged && btnPlus.pressed) plusPressedAt = millis();

    if (btnPlus.pressed && btnNext.pressed && !gestureBoth) {
      gestureBoth = true;
      onBothButtons();
    }

    // Утримуємо лише «+» — значення біжить саме.
    if (btnPlus.pressed && !gestureNext && uiMode != UI_HOME &&
        millis() - plusPressedAt >= REPEAT_DELAY_MS &&
        millis() - lastRepeatAt >= REPEAT_EVERY_MS) {
      lastRepeatAt = millis();
      plusRepeated = true;
      onPlus();
    }
    return;
  }

  if (gestureActive) {  // обидві відпущені — жест завершено
    gestureActive = false;
    if (gestureBoth || plusRepeated) return;
    if (gesturePlus) onPlus();
    else if (gestureNext) onNext();
  }
}

// ---------------------------------------------------------------------------
// Команди з Serial Monitor (як в уроці 2)

void handleCommand(char *line) {
  // Переводимо команду у верхній регістр, щоб працювало і «set», і «SET».
  for (char *p = line; *p; p++) {
    if (*p >= 'a' && *p <= 'z') *p -= 32;
  }

  int y, mo, d, h, mi, s, track;

  if (strcmp(line, "HELP") == 0) {
    printHelp();
  } else if (strcmp(line, "NOW") == 0) {
    DateTime now;
    if (readClock(now)) {
      printClockLine(now);
    } else {
      Serial.println(F("Час недоступний (див. повідомлення вище)."));
    }
  } else if (strncmp(line, "SET ", 4) == 0) {
    if (sscanf(line + 4, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) {
      Serial.println(F("Формат: SET 2026-10-16 08:59:50"));
      return;
    }
    DateTime t(y, mo, d, h, mi, s);
    if (!timeLooksValid(t) || h > 23 || mi > 59 || s > 59) {
      Serial.println(F("ПОМИЛКА: неправильна дата або час."));
      return;
    }
    if (!i2cResponds(RTC_I2C_ADDRESS)) {
      setClockState(CLOCK_MISSING);
      Serial.println(F("ПОМИЛКА: годинник не відповідає, час не записано."));
      return;
    }
    if (clockState == CLOCK_MISSING) rtc.begin();
    rtc.adjust(t);
    Serial.print(F("Годинник встановлено: "));
    printDateTime(t);
    Serial.println();
    clockState = CLOCK_NOT_SET;  // перевіримо, що час справді читається
    clockReady = readClock(lastNow);
    screenDirty = true;
  } else if (strcmp(line, "SCHEDULE") == 0) {
    printSchedule();
  } else if (strcmp(line, "PLAY") == 0) {
    startTrack(1);
  } else if (strncmp(line, "PLAY ", 5) == 0) {
    track = atoi(line + 5);
    if (track < 1) {
      Serial.println(F("Формат: PLAY 2"));
      return;
    }
    startTrack(track);
  } else if (strcmp(line, "STOP") == 0) {
    if (playerState != STATE_NO_PLAYER) {
      pendingTrack = 0;  // скасовуємо й подію, що чекала зупинки
      if (playerState == STATE_STOPPING) playerState = STATE_PLAYING;
      player.stop();
      Serial.println(F("Команда: стоп."));
    }
  } else if (strcmp(line, "CLOCK ON") == 0) {
    printEverySecond = true;
  } else if (strcmp(line, "CLOCK OFF") == 0) {
    printEverySecond = false;
    Serial.println(F("Друк часу щосекунди вимкнено (NOW — показати час)."));
  } else if (line[0] != '\0') {
    Serial.print(F("Невідома команда: "));
    Serial.println(line);
    printHelp();
  }
}

// Читає символи без очікування: loop() не зупиняється, поки людина друкує.
void readSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      commandLine[commandLength] = '\0';
      handleCommand(commandLine);
      commandLength = 0;
    } else if (commandLength < sizeof(commandLine) - 1) {
      commandLine[commandLength++] = c;
    }
  }
}

// ---------------------------------------------------------------------------

void setupPlayer() {
  Serial.println(F("Запуск DFPlayer..."));
  // begin() скидає модуль і чекає до ~2 с — блокує лише один раз під час старту.
  if (!player.begin(mp3Serial, true, true)) {
    Serial.println(F("ПОМИЛКА: DFPlayer не відповідає або немає SD-карти."));
    playerState = STATE_NO_PLAYER;
    return;
  }
  Serial.println(F("DFPlayer готовий."));

  // Кількість файлів питаємо одразу після begin() (див. урок 1).
  fileCount = player.readFileCounts();
  if (fileCount < 0) {
    Serial.println(F("Увага: не вдалося прочитати кількість файлів на SD."));
  } else {
    Serial.print(F("Файлів на SD: "));
    Serial.println(fileCount);
  }
  player.volume(VOLUME);
  playerState = STATE_READY;
}

void setup() {
  Serial.begin(9600);
  mp3Serial.begin(9600);
  Wire.begin();
  pinMode(PIN_MP3_BUSY, INPUT_PULLUP);
  pinMode(PIN_BTN_PLUS, INPUT_PULLUP);
  pinMode(PIN_BTN_NEXT, INPUT_PULLUP);

  Serial.println(F("Урок 3: Uno + DFPlayer + DS3231 + OLED + 2 кнопки"));

  // Дисплей вмикаємо першим, щоб під час старту DFPlayer (до 2 с) було що показати.
  checkDisplay();
  unsigned long logoAt = millis();
  if (!displayOk) {
    Serial.println(F("ПОМИЛКА: OLED не відповідає на 0x3C. Пристрій працює, але без екрана."));
  } else {
    showLogo();  // логотип висить, поки стартують плеєр і годинник
  }

  setupPlayer();
  loadSettings();
  printSchedule();

  if (!i2cResponds(RTC_I2C_ADDRESS)) {
    printClockState(CLOCK_MISSING);
  }
  clockReady = readClock(lastNow);
  if (clockReady) {
    Serial.print(F("Зараз: "));
    printDateTime(lastNow);
    Serial.println();
  }
  // Зерно для random(): шум вільного піна A0 (кілька читань) + час старту + годинник.
  uint32_t seed = micros() ^ (clockReady ? lastNow.unixtime() : 0);
  for (uint8_t k = 0; k < 16; k++) seed = seed * 31 + analogRead(A0);
  randomSeed(seed);

  printHelp();

  // Логотип має побути на екрані хоча б LOGO_MS. Зазвичай DFPlayer стартує
  // довше, і чекати не треба. Це очікування — лише один раз, під час запуску.
  if (displayOk) {
    while (millis() - logoAt < LOGO_MS) {
    }
    resetScreen();
  }
  screenDirty = true;
}

void loop() {
  readSerialCommands();
  readButtons();
  updatePlayer();

  // Раз на секунду: годинник, розклад, перевірка дисплея.
  if (millis() - lastClockCheckAt >= CLOCK_CHECK_MS) {
    lastClockCheckAt = millis();
    checkDisplay();
    clockReady = readClock(lastNow);
    if (clockReady) {
      if (printEverySecond) printClockLine(lastNow);
      checkSchedule(lastNow);
    }
    screenDirty = true;
  }

  if (noticeIcon != ICON_NONE && millis() - noticeAt >= NOTICE_MS) {
    noticeIcon = ICON_NONE;
    screenDirty = true;
  }

  if (uiMode != UI_HOME && millis() - lastUiActionAt >= MENU_TIMEOUT_MS) {
    Serial.println(F("Меню закрито без збереження (30 с без натискань)."));
    leaveMenu();
  }

  if (screenDirty) {
    screenDirty = false;
    renderScreen();
  }
}
