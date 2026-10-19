// Урок 2 — Make it know the time ⏰
// Arduino Uno + DFPlayer Mini (з уроку 1) + годинник реального часу DS3231.
// Мета: треки запускаються САМІ за шкільним розкладом.
//
// Схема (див. ../README.md та ../wokwi/diagram.json):
//   DFPlayer VCC  → Uno 5V           DS3231 VCC → Uno 5V
//   DFPlayer GND  → Uno GND          DS3231 GND → Uno GND
//   DFPlayer TX   → Uno D10          DS3231 SDA → Uno A4 (SDA)
//   DFPlayer RX   ← 1 кОм ← Uno D11  DS3231 SCL → Uno A5 (SCL)
//   DFPlayer BUSY → Uno D7  (LOW = трек грає)
//   DFPlayer SPK_1/SPK_2 → динамік 4–8 Ом, до 3 Вт
//
// Бібліотеки (Arduino Library Manager): DFRobotDFPlayerMini, RTClib (Adafruit).
//
// Розклад зберігається в EEPROM. Поки що його задає функція setupSchedule()
// у setup() з таблиці SCHEDULE_FROM_CODE нижче. Пізніше налаштування
// прийдуть ззовні (застосунок / команди) і записуватимуться в ту саму EEPROM.
//
// Керування — текстові команди в Serial Monitor (9600 бод, «Новий рядок»):
//   HELP                          — список команд
//   NOW                           — показати поточний час один раз
//   SET 2026-10-16 08:59:55       — встановити годинник
//   SCHEDULE                      — показати розклад з EEPROM
//   PLAY / PLAY 2                 — запустити трек вручну (перевірка звуку)
//   STOP                          — зупинити відтворення
//   CLOCK ON / CLOCK OFF          — друкувати час щосекунди чи ні
//
// Правила чесності:
//   * «ГРАЄ» друкуємо лише після підтвердження BUSY = LOW.
//   * Якщо годинник недоступний або час не встановлено — показуємо помилку
//     і НЕ запускаємо події у випадковий час.
//   * Кожна подія спрацьовує один раз на день: дата останнього запуску
//     зберігається в EEPROM, тож перезапуск Arduino її не повторить.
//   * Якщо настав час події, а попередній трек ще грає (хвилина мовчання
//     о 09:00 → гімн о 09:01), подія чекає в черзі й стартує одразу після нього.

#include <Wire.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <RTClib.h>
#include <DFRobotDFPlayerMini.h>

// --- Піни (Arduino Uno) ---
const uint8_t PIN_MP3_RX = 10;   // Uno приймає ← TX DFPlayer
const uint8_t PIN_MP3_TX = 11;   // Uno передає → 1 кОм → RX DFPlayer
const uint8_t PIN_MP3_BUSY = 7;  // LOW, поки трек грає
// DS3231 підключений до апаратної шини I²C: A4 = SDA, A5 = SCL.
const uint8_t RTC_I2C_ADDRESS = 0x68;

// --- Налаштування ---
const uint8_t VOLUME = 20;                           // 0..30
const unsigned long PLAY_CONFIRM_TIMEOUT_MS = 1500;  // скільки чекаємо BUSY = LOW
const unsigned long CLOCK_CHECK_MS = 1000;           // як часто читаємо годинник
const uint16_t MIN_VALID_YEAR = 2025;                // раніше — час точно не встановлено

// --- Шкільний розклад (поки що задається тут) ---
// Нумерація файлів — з картки US-5 у Trello:
//   0001.mp3 — гімн України, 0002.mp3 — хвилина мовчання, 0003.mp3 — цитата дня.
struct EventConfig {
  uint8_t hour;
  uint8_t minute;
  uint8_t track;
};

const EventConfig SCHEDULE_FROM_CODE[] = {
  { 15,  39, 4},  // 09:00 — хвилина мовчання
  { 15,  40, 2},  // 09:01 — гімн України (після хвилини мовчання)
  {12, 10, 3},  // 12:10 — цитата дня, велика перерва
};
const uint8_t SCHEDULE_FROM_CODE_COUNT = sizeof(SCHEDULE_FROM_CODE) / sizeof(SCHEDULE_FROM_CODE[0]);

// --- EEPROM: розклад і дати останніх запусків переживають перезапуск ---
const int EEPROM_ADDR = 0;
const uint8_t EEPROM_MAGIC = 0xB3;  // «підпис» саме цього формату даних
const uint8_t MAX_EVENTS = 8;

struct ScheduleEvent {
  uint8_t hour;
  uint8_t minute;
  uint8_t track;
  uint32_t lastFiredDay;  // РРРРММДД дня, коли подія вже спрацювала (0 = ще ні)
};

struct Settings {
  uint8_t magic;
  uint8_t count;
  ScheduleEvent events[MAX_EVENTS];
};

SoftwareSerial mp3Serial(PIN_MP3_RX, PIN_MP3_TX);
DFRobotDFPlayerMini player;
RTC_DS3231 rtc;

// Стани плеєра (як в уроці 1)
enum PlayerState {
  STATE_NO_PLAYER,  // модуль не відповів або немає SD-карти
  STATE_READY,      // готовий, нічого не грає
  STATE_STARTING,   // команду play надіслано, чекаємо підтвердження
  STATE_PLAYING     // BUSY = LOW, звук підтверджено
};

// Стани годинника
enum ClockState {
  CLOCK_MISSING,  // DS3231 не відповідає на шині I²C
  CLOCK_NOT_SET,  // відповідає, але час не встановлено / збився
  CLOCK_OK        // час можна використовувати для розкладу
};

PlayerState playerState = STATE_NO_PLAYER;
ClockState clockState = CLOCK_MISSING;
Settings settings;

int fileCount = -1;        // скільки файлів на SD (-1 = невідомо)
int8_t pendingEvent = -1;  // подія, що чекає, поки звільниться плеєр (-1 = немає)
unsigned long startRequestedAt = 0;
unsigned long lastClockCheckAt = 0;
bool printEverySecond = true;

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

void printEvent(uint8_t i) {
  const ScheduleEvent &e = settings.events[i];
  Serial.print(i + 1);
  Serial.print(F(") "));
  print2(e.hour);
  Serial.print(':');
  print2(e.minute);
  Serial.print(F(" → трек "));
  Serial.print(e.track);
}

void printSchedule() {
  Serial.print(F("Розклад (EEPROM), подій: "));
  Serial.println(settings.count);
  for (uint8_t i = 0; i < settings.count; i++) {
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
  Serial.println(F("Команди: NOW | SET РРРР-ММ-ДД ГГ:ХХ:СС | SCHEDULE"));
  Serial.println(F("         PLAY [трек] | STOP | CLOCK ON | CLOCK OFF | HELP"));
}

// ---------------------------------------------------------------------------
// EEPROM і розклад

void saveSettings() {
  EEPROM.put(EEPROM_ADDR, settings);  // put() перезаписує лише змінені байти
}

bool settingsLookValid() {
  if (settings.magic != EEPROM_MAGIC || settings.count > MAX_EVENTS) return false;
  for (uint8_t i = 0; i < settings.count; i++) {
    const ScheduleEvent &e = settings.events[i];
    if (e.hour > 23 || e.minute > 59 || e.track == 0) return false;
  }
  return true;
}

void loadSettings() {
  EEPROM.get(EEPROM_ADDR, settings);
  if (!settingsLookValid()) {
    settings.magic = 0;  // EEPROM порожня або старого формату
    settings.count = 0;
  }
}

// Тимчасовий спосіб налаштування: записує розклад із коду в EEPROM.
// Якщо в EEPROM уже такий самий розклад — нічого не змінює, тож дати
// останніх запусків зберігаються і подія не повториться після RESET.
void setupSchedule() {
  bool same = settings.magic == EEPROM_MAGIC && settings.count == SCHEDULE_FROM_CODE_COUNT;
  for (uint8_t i = 0; same && i < SCHEDULE_FROM_CODE_COUNT; i++) {
    const EventConfig &c = SCHEDULE_FROM_CODE[i];
    const ScheduleEvent &e = settings.events[i];
    same = e.hour == c.hour && e.minute == c.minute && e.track == c.track;
  }
  if (same) {
    Serial.println(F("Розклад в EEPROM збігається з кодом."));
    return;
  }

  settings.magic = EEPROM_MAGIC;
  settings.count = SCHEDULE_FROM_CODE_COUNT;
  for (uint8_t i = 0; i < SCHEDULE_FROM_CODE_COUNT; i++) {
    settings.events[i].hour = SCHEDULE_FROM_CODE[i].hour;
    settings.events[i].minute = SCHEDULE_FROM_CODE[i].minute;
    settings.events[i].track = SCHEDULE_FROM_CODE[i].track;
    settings.events[i].lastFiredDay = 0;
  }
  saveSettings();
  Serial.println(F("Розклад із коду записано в EEPROM."));
}

uint32_t dayKey(const DateTime &t) {
  return (uint32_t)t.year() * 10000UL + t.month() * 100UL + t.day();
}

// ---------------------------------------------------------------------------
// Годинник DS3231

bool rtcResponds() {
  Wire.beginTransmission(RTC_I2C_ADDRESS);
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
  if (!rtcResponds()) {
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

// Скільки секунд до найближчої події (через північ — до завтрашньої).
// Повертає -1, якщо розклад порожній; у nextIndex — номер події.
long secondsUntilNext(const DateTime &now, int8_t &nextIndex) {
  long nowSec = now.hour() * 3600L + now.minute() * 60L + now.second();
  long best = -1;
  nextIndex = -1;
  for (uint8_t i = 0; i < settings.count; i++) {
    long diff = settings.events[i].hour * 3600L + settings.events[i].minute * 60L - nowSec;
    if (diff < 0) diff += 86400L;
    if (best < 0 || diff < best) {
      best = diff;
      nextIndex = i;
    }
  }
  return best;
}

void printClockLine(const DateTime &now) {
  Serial.print(F("Час: "));
  printDateTime(now);
  int8_t next;
  long left = secondsUntilNext(now, next);
  if (left >= 0) {
    Serial.print(F(" | до події "));
    Serial.print(next + 1);
    Serial.print(F(": "));
    print2(left / 3600);
    Serial.print(':');
    print2((left / 60) % 60);
    Serial.print(':');
    print2(left % 60);
  }
  if (playerState == STATE_PLAYING) Serial.print(F(" | ГРАЄ"));
  Serial.println();
}

// ---------------------------------------------------------------------------
// DFPlayer (логіка з уроку 1, але трек тепер — параметр)

bool isBusyLow() {
  return digitalRead(PIN_MP3_BUSY) == LOW;
}

void startTrack(uint16_t track) {
  if (playerState == STATE_NO_PLAYER) {
    Serial.println(F("ПОМИЛКА: DFPlayer недоступний — відтворення неможливе."));
    return;
  }
  if (fileCount >= 0 && fileCount < (int)track) {
    Serial.print(F("ПОМИЛКА: на SD-карті немає треку "));
    Serial.println(track);
    return;
  }
  Serial.print(F("Команда: відтворити трек "));
  Serial.println(track);
  player.play(track);
  startRequestedAt = millis();
  playerState = STATE_STARTING;
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
        Serial.println(F("ГРАЄ: відтворення підтверджено (BUSY = LOW)."));
      } else if (millis() - startRequestedAt >= PLAY_CONFIRM_TIMEOUT_MS) {
        playerState = STATE_READY;
        Serial.println(F("ПОМИЛКА: трек не запустився (немає підтвердження BUSY)."));
      }
      break;

    case STATE_PLAYING:
      if (!isBusyLow()) {
        playerState = STATE_READY;
        Serial.println(F("ГОТОВО: трек більше не грає."));
      }
      break;

    default:
      break;
  }

  // Плеєр звільнився — запускаємо подію, яка чекала в черзі.
  if (playerState == STATE_READY && pendingEvent >= 0) {
    uint8_t i = pendingEvent;
    pendingEvent = -1;
    Serial.print(F("З черги: подія "));
    printEvent(i);
    Serial.println();
    startTrack(settings.events[i].track);
  }
}

// ---------------------------------------------------------------------------
// Розклад: кожна подія — один раз на день

bool playerBusy() {
  return playerState == STATE_STARTING || playerState == STATE_PLAYING;
}

void checkSchedule(const DateTime &now) {
  uint32_t today = dayKey(now);
  for (uint8_t i = 0; i < settings.count; i++) {
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

    if (!playerBusy()) {
      startTrack(e.track);
    } else if (pendingEvent < 0) {
      pendingEvent = i;
      Serial.println(F("Плеєр зайнятий — подія чекає, поки завершиться поточний трек."));
    } else {
      Serial.println(F("ПРОПУЩЕНО: плеєр зайнятий, у черзі вже є подія."));
    }
  }
}

// ---------------------------------------------------------------------------
// Команди з Serial Monitor

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
    if (!rtcResponds()) {
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
    DateTime now;
    readClock(now);
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
      pendingEvent = -1;
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

  Serial.println(F("Урок 2: Arduino Uno + DFPlayer Mini + DS3231"));
  setupPlayer();

  loadSettings();
  setupSchedule();  // тимчасово: розклад із коду → EEPROM
  printSchedule();

  // Перше читання годинника. clockState = CLOCK_MISSING, тож readClock()
  // сам викличе rtc.begin(), якщо модуль відповідає.
  if (!rtcResponds()) {
    printClockState(CLOCK_MISSING);
  }
  DateTime now;
  if (readClock(now)) {
    Serial.print(F("Зараз: "));
    printDateTime(now);
    Serial.println();
  }
  printHelp();
}

void loop() {
  readSerialCommands();
  updatePlayer();

  // Раз на секунду: читаємо годинник, друкуємо час, перевіряємо розклад.
  if (millis() - lastClockCheckAt >= CLOCK_CHECK_MS) {
    lastClockCheckAt = millis();
    DateTime now;
    if (readClock(now)) {
      if (printEverySecond) printClockLine(now);
      checkSchedule(now);
    }
  }
}
