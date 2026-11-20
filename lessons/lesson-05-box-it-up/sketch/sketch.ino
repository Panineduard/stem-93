// Урок 5 — Збери в корпус 🧱  · Скетч «САМОПЕРЕВІРКА»
// Arduino Uno + DFPlayer Mini + DS3231 + OLED SSD1306 + дві кнопки + модуль реле.
//
// Навіщо: ми перепаяли всі дроти й заховали плату в коробку. Тепер треба швидко
// дізнатися, ЩО саме працює, а що ні, і не розбирати коробку навмання.
// Скетч по черзі перевіряє кожен модуль і пише результат на екран і в Serial Monitor.
//
// ⚠ Це ОКРЕМА прошивка для перевірки. Після тесту залийте назад робочий скетч
//   уроку 4 (../../lesson-04-wake-the-amp/sketch). EEPROM цей скетч НЕ чіпає,
//   тому ваш розклад (хвилина мовчання, цитата дня) залишиться на місці.
//
// Схема та сама, що в уроці 4:
//   DFPlayer TX → D10, RX ← 1 кОм ← D11, BUSY → D7 (LOW = трек грає)
//   DS3231 і OLED: SDA → A4, SCL → A5 (годинник 0x68, дисплей 0x3C)
//   Кнопка A «+» → D2 і GND,  кнопка B «→» → D3 і GND (INPUT_PULLUP)
//   Модуль реле: IN → D4 (наш модуль вмикається рівнем LOW)
//
// Бібліотеки: DFRobotDFPlayerMini, RTClib (Adafruit), U8g2 — ті самі, що в уроці 4.
//
// Що на екрані (8 рядків по 16 знаків; кирилиці у шрифті U8x8 немає, тому латиниця):
//   SELF-TEST         ← заголовок; наприкінці «ALL OK :)» або «CHECK  n»
//   OLED  OK          ← якщо ви це читаєте, дисплей живий 🙂
//   RTC   12:41:07    ← годинник відповідає і цокає | NO — не відповідає | TIME? — час збився
//   MP3   OK          ← DFPlayer відповів | NO — модуль або SD не відповідає
//   SD    12 FILES    ← скільки файлів на карті | EMPTY — карта порожня
//   SOUND OK          ← BUSY став LOW: трек СПРАВДІ грає | NO — не підтвердилось
//   RELAY CLICK?      ← реле смикнули; чи клацнуло — перевіряєте ВИ (вухом і світлодіодом)
//   BTN   A- B-       ← натисніть кожну кнопку: стане A+ B+ | STUCK — кнопка «залипла»
//
// Кнопки під час тесту:
//   A      — позначити кнопку A і ще раз програти звук;
//   B      — позначити кнопку B і ще раз клацнути реле;
//   A + B  — пройти весь тест спочатку.
//
// Правила чесності (з попередніх уроків):
//   * SOUND OK — лише після підтвердження BUSY = LOW. Немає підтвердження — NO.
//   * Реле не повідомляє, чи воно справді клацнуло. Тому CLICK?, а не OK.
//   * Усі очікування — на millis(), без delay(): кнопки відповідають завжди.

#include <Wire.h>
#include <SoftwareSerial.h>
#include <RTClib.h>
#include <DFRobotDFPlayerMini.h>
#include <U8x8lib.h>

// --- Піни (ті самі, що в уроці 4) ---
const uint8_t PIN_MP3_RX = 10;
const uint8_t PIN_MP3_TX = 11;
const uint8_t PIN_MP3_BUSY = 7;
const uint8_t PIN_BTN_A = 2;
const uint8_t PIN_BTN_B = 3;
const uint8_t PIN_RELAY = 4;
const bool RELAY_ACTIVE_LOW = true;  // як в уроці 4: LOW на D4 = реле УВІМК

const uint8_t RTC_I2C_ADDRESS = 0x68;
const uint8_t OLED_I2C_ADDRESS = 0x3C;

// --- Налаштування тесту ---
const uint8_t VOLUME = 18;                          // 0..30, трохи тихіше, ніж у робочому скетчі
const uint16_t TEST_TRACK = 7;                      // перша цитата дня (гімн для тесту не вмикаємо)
const unsigned long RELAY_CLICK_MS = 1500;          // скільки тримаємо реле, щоб почути «клац-клац»
const unsigned long RTC_TICK_WAIT_MS = 1200;        // за цей час секунди годинника мають змінитися
const unsigned long PLAY_CONFIRM_TIMEOUT_MS = 1500; // скільки чекаємо BUSY = LOW
const unsigned long SOUND_MS = 4000;                // скільки грає тестовий звук
const unsigned long DEBOUNCE_MS = 30;
const uint16_t MIN_VALID_YEAR = 2025;

// Якщо екран показує «сміття» або картинка зсунута на 2 пікселі — можливо,
// на вашому модулі контролер SH1106, а не SSD1306. Тоді замініть рядок нижче на:
//   U8X8_SH1106_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);
SoftwareSerial mp3Serial(PIN_MP3_RX, PIN_MP3_TX);
DFRobotDFPlayerMini player;
RTC_DS3231 rtc;

// Рядки екрана
enum Row { ROW_TITLE, ROW_OLED, ROW_RTC, ROW_MP3, ROW_SD, ROW_SOUND, ROW_RELAY, ROW_BTN };

// Результат однієї перевірки
enum Result : uint8_t { R_WAIT, R_OK, R_FAIL, R_ASK };

// Кроки тесту — простий автомат станів (як меню в уроці 3)
enum Step : uint8_t {
  STEP_RTC_FIRST,   // читаємо годинник уперше
  STEP_RTC_TICK,    // чекаємо, чи змінилися секунди
  STEP_RELAY_ON,    // реле увімкнули, тримаємо
  STEP_SOUND_WAIT,  // попросили трек, чекаємо BUSY = LOW
  STEP_SOUND_PLAY,  // звук іде, даємо послухати
  STEP_DONE
};

Step step = STEP_DONE;
unsigned long stepAt = 0;

bool displayOk = false;
bool playerOk = false;
int fileCount = -1;
uint8_t firstSecond = 0;
Result resRtc = R_WAIT, resMp3 = R_WAIT, resSound = R_WAIT, resRelay = R_WAIT;
bool rtcTimeBad = false;
bool btnSeen[2] = { false, false };
bool btnStuck[2] = { false, false };

struct Button {
  uint8_t pin;
  bool pressed;
  bool raw;
  unsigned long changedAt;
};
Button btnA = { PIN_BTN_A, false, false, 0 };
Button btnB = { PIN_BTN_B, false, false, 0 };
bool bothHandled = false;

// ---------------------------------------------------------------------------
// Дрібні помічники

bool i2cResponds(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void writeRelay(bool on) {
  digitalWrite(PIN_RELAY, on != RELAY_ACTIVE_LOW ? HIGH : LOW);
}

bool busyPlaying() {
  return digitalRead(PIN_MP3_BUSY) == LOW;
}

// Рядок екрана: назва (6 знаків) + значення. Інші знаки затираємо пробілами.
void showRow(uint8_t row, const __FlashStringHelper *label, const char *value) {
  if (!displayOk) return;
  char line[17];
  char lbl[7];
  strncpy_P(lbl, (const char *)label, sizeof(lbl));
  lbl[6] = '\0';
  snprintf(line, sizeof(line), "%-6s%-10s", lbl, value);
  oled.setInverseFont(row == ROW_TITLE);
  oled.drawString(0, row, line);
  oled.setInverseFont(0);
}

const char *resultText(Result r) {
  switch (r) {
    case R_OK: return "OK";
    case R_FAIL: return "NO";
    case R_ASK: return "CLICK?";
    default: return "..";
  }
}

// ---------------------------------------------------------------------------
// Екран

void drawTitle() {
  if (step != STEP_DONE) {
    showRow(ROW_TITLE, F("SELF-"), "TEST");
    return;
  }
  // Скільки перевірок ще не зелені?
  uint8_t bad = 0;
  if (resRtc != R_OK) bad++;
  if (resMp3 != R_OK) bad++;
  if (resSound != R_OK) bad++;
  if (!btnSeen[0] || !btnSeen[1] || btnStuck[0] || btnStuck[1]) bad++;
  char v[11];
  if (bad == 0) {
    showRow(ROW_TITLE, F("ALL"), "OK :)");
  } else {
    snprintf(v, sizeof(v), "%u", bad);
    showRow(ROW_TITLE, F("CHECK"), v);
  }
}

void drawRtc() {
  if (resRtc == R_FAIL && rtcTimeBad) {
    showRow(ROW_RTC, F("RTC"), "TIME?");
  } else if (resRtc == R_OK) {
    DateTime now = rtc.now();
    char v[11];
    snprintf(v, sizeof(v), "%02u:%02u:%02u", now.hour(), now.minute(), now.second());
    showRow(ROW_RTC, F("RTC"), v);
  } else {
    showRow(ROW_RTC, F("RTC"), resultText(resRtc));
  }
}

void drawSd() {
  char v[11];
  if (!playerOk) {
    showRow(ROW_SD, F("SD"), "--");
  } else if (fileCount <= 0) {
    showRow(ROW_SD, F("SD"), fileCount == 0 ? "EMPTY" : "?");
  } else {
    snprintf(v, sizeof(v), "%d FILES", fileCount);
    showRow(ROW_SD, F("SD"), v);
  }
}

void drawButtons() {
  char v[11];
  for (uint8_t i = 0; i < 2; i++) {
    if (btnStuck[i]) {
      snprintf(v, sizeof(v), "%c STUCK", i == 0 ? 'A' : 'B');
      showRow(ROW_BTN, F("BTN"), v);
      return;
    }
  }
  snprintf(v, sizeof(v), "A%c B%c", btnSeen[0] ? '+' : '-', btnSeen[1] ? '+' : '-');
  showRow(ROW_BTN, F("BTN"), v);
}

void drawAll() {
  drawTitle();
  showRow(ROW_OLED, F("OLED"), "OK");
  drawRtc();
  showRow(ROW_MP3, F("MP3"), resultText(resMp3));
  drawSd();
  showRow(ROW_SOUND, F("SOUND"), resultText(resSound));
  showRow(ROW_RELAY, F("RELAY"), resultText(resRelay));
  drawButtons();
}

// ---------------------------------------------------------------------------
// Перевірки

void checkDisplay() {
  displayOk = i2cResponds(OLED_I2C_ADDRESS);
  if (displayOk) {
    oled.begin();
    oled.setPowerSave(0);
    oled.setFont(u8x8_font_chroma48medium8_r);
    oled.clearDisplay();
    Serial.println(F("[OLED ] OK: дисплей відповідає на 0x3C."));
  } else {
    Serial.println(F("[OLED ] НЕМАЄ: 0x3C мовчить. Перевірте SDA→A4, SCL→A5, VCC, GND дисплея."));
    Serial.println(F("        Решта тесту йде далі — дивіться результати тут, у Serial Monitor."));
  }
}

void checkPlayer() {
  Serial.println(F("[MP3  ] Запуск DFPlayer (до 2 с)..."));
  // begin() — єдине блокувальне очікування, лише один раз (як в уроці 4).
  playerOk = player.begin(mp3Serial, true, true);
  if (!playerOk) {
    resMp3 = R_FAIL;
    fileCount = -1;
    Serial.println(F("[MP3  ] НЕМАЄ: DFPlayer не відповідає або немає SD-карти."));
    Serial.println(F("        Перевірте: TX DFPlayer→D10, RX DFPlayer←1 кОм←D11, 5V, GND, карту в слоті."));
    return;
  }
  resMp3 = R_OK;
  fileCount = player.readFileCounts();
  player.volume(VOLUME);
  Serial.print(F("[MP3  ] OK. Файлів на SD: "));
  Serial.println(fileCount);
  if (fileCount == 0) Serial.println(F("[SD   ] Карта порожня — скопіюйте MP3-файли (див. урок 3)."));
}

void startTest() {
  Serial.println();
  Serial.println(F("===== САМОПЕРЕВІРКА: старт ====="));
  writeRelay(false);
  if (playerOk) player.stop();
  resRtc = R_WAIT;
  resSound = R_WAIT;
  resRelay = R_WAIT;
  rtcTimeBad = false;
  step = STEP_RTC_FIRST;
  stepAt = millis();
  if (displayOk) oled.clearDisplay();
  drawAll();
}

void startSound() {
  if (!playerOk) {
    resSound = R_FAIL;
    Serial.println(F("[SOUND] Пропуск: DFPlayer не відповідає."));
    step = STEP_DONE;
    return;
  }
  uint16_t track = (fileCount >= (int)TEST_TRACK) ? TEST_TRACK : 1;
  // Підсилювач (реле) вмикаємо разом зі звуком — як у робочому скетчі.
  writeRelay(true);
  player.play(track);
  Serial.print(F("[SOUND] Граю трек "));
  Serial.print(track);
  Serial.println(F(", чекаю підтвердження BUSY = LOW..."));
  resSound = R_WAIT;
  step = STEP_SOUND_WAIT;
  stepAt = millis();
}

void startRelayClick() {
  writeRelay(true);
  resRelay = R_WAIT;
  Serial.println(F("[RELAY] Реле УВІМК. Слухайте «клац» і дивіться на світлодіод модуля..."));
  step = STEP_RELAY_ON;
  stepAt = millis();
}

void finish() {
  writeRelay(false);
  step = STEP_DONE;
  Serial.println(F("===== САМОПЕРЕВІРКА: готово ====="));
  Serial.println(F("Натисніть A (звук ще раз), B (реле ще раз), A+B (увесь тест спочатку)."));
}

// Автомат станів: кожен виклик робить лише маленький крок і одразу повертається.
void updateTest() {
  unsigned long elapsed = millis() - stepAt;
  switch (step) {
    case STEP_RTC_FIRST: {
      if (!i2cResponds(RTC_I2C_ADDRESS) || !rtc.begin()) {
        resRtc = R_FAIL;
        Serial.println(F("[RTC  ] НЕМАЄ: 0x68 мовчить. Перевірте SDA→A4, SCL→A5, VCC, GND годинника."));
        startRelayClick();
      } else {
        firstSecond = rtc.now().second();
        step = STEP_RTC_TICK;
        stepAt = millis();
      }
      drawAll();
      break;
    }
    case STEP_RTC_TICK:
      if (elapsed >= RTC_TICK_WAIT_MS) {
        DateTime now = rtc.now();
        bool ticks = now.second() != firstSecond;
        bool valid = now.isValid() && now.year() >= MIN_VALID_YEAR && now.year() <= 2099 && !rtc.lostPower();
        if (ticks && valid) {
          resRtc = R_OK;
          Serial.println(F("[RTC  ] OK: годинник відповідає і цокає."));
        } else {
          resRtc = R_FAIL;
          rtcTimeBad = true;
          Serial.println(ticks ? F("[RTC  ] ЧАС ЗБИВСЯ: годинник цокає, але час не встановлено. Команда SET в уроці 4.")
                               : F("[RTC  ] СТОЇТЬ: секунди не змінюються. Перевірте батарейку CR2032."));
        }
        startRelayClick();
        drawAll();
      }
      break;
    case STEP_RELAY_ON:
      if (elapsed >= RELAY_CLICK_MS) {
        writeRelay(false);
        resRelay = R_ASK;
        Serial.println(F("[RELAY] Реле ВИМК. Чули два клацання? Так — реле живе. Ні — перевірте IN→D4, VCC, GND."));
        startSound();
        drawAll();
      }
      break;
    case STEP_SOUND_WAIT:
      if (busyPlaying()) {
        resSound = R_OK;
        Serial.println(F("[SOUND] OK: відтворення підтверджено (BUSY = LOW). Чуєте звук?"));
        Serial.println(F("        Якщо BUSY є, а звуку немає — перевірте дроти динаміка SPK_1/SPK_2."));
        step = STEP_SOUND_PLAY;
        stepAt = millis();
        drawAll();
      } else if (elapsed >= PLAY_CONFIRM_TIMEOUT_MS) {
        resSound = R_FAIL;
        Serial.println(F("[SOUND] НЕ ПІДТВЕРДЖЕНО: BUSY не став LOW. Перевірте BUSY→D7 і чи є такий трек на SD."));
        player.stop();
        finish();
        drawAll();
      }
      break;
    case STEP_SOUND_PLAY:
      if (elapsed >= SOUND_MS || !busyPlaying()) {
        player.stop();
        finish();
        drawAll();
      }
      break;
    case STEP_DONE:
      break;
  }
}

// ---------------------------------------------------------------------------
// Кнопки (антибрязкіт як в уроці 3)

bool updateButton(Button &b) {
  bool raw = digitalRead(b.pin) == LOW;
  if (raw != b.raw) {
    b.raw = raw;
    b.changedAt = millis();
  }
  if (b.pressed != b.raw && millis() - b.changedAt >= DEBOUNCE_MS) {
    b.pressed = b.raw;
    return b.pressed;  // true — лише в мить натискання
  }
  return false;
}

void readButtons() {
  bool downA = updateButton(btnA);
  bool downB = updateButton(btnB);

  if (btnA.pressed && btnB.pressed) {
    if (!bothHandled) {
      bothHandled = true;
      Serial.println(F("[BTN  ] A+B: тест спочатку."));
      startTest();
    }
    return;
  }
  if (!btnA.pressed && !btnB.pressed) bothHandled = false;
  if (bothHandled) return;

  if (downA) {
    btnSeen[0] = true;
    Serial.println(F("[BTN  ] Кнопка A працює."));
    drawButtons();
    drawTitle();
    if (step == STEP_DONE) startSound();
  }
  if (downB) {
    btnSeen[1] = true;
    Serial.println(F("[BTN  ] Кнопка B працює."));
    drawButtons();
    drawTitle();
    if (step == STEP_DONE) startRelayClick();
  }
}

// Кнопка, натиснута вже під час увімкнення, швидше за все «залипла»
// або її дріт замкнуло на GND під час пайки.
void checkStuckButtons() {
  btnStuck[0] = digitalRead(PIN_BTN_A) == LOW;
  btnStuck[1] = digitalRead(PIN_BTN_B) == LOW;
  for (uint8_t i = 0; i < 2; i++) {
    if (btnStuck[i]) {
      Serial.print(F("[BTN  ] УВАГА: кнопка "));
      Serial.print(i == 0 ? 'A' : 'B');
      Serial.println(F(" натиснута під час старту. Залипла або дріт торкається GND?"));
    }
  }
}

// ---------------------------------------------------------------------------

void setup() {
  // Найперше — реле ВИМКНЕНЕ (безпечний стан, як в уроці 4).
  writeRelay(false);
  pinMode(PIN_RELAY, OUTPUT);

  Serial.begin(9600);
  mp3Serial.begin(9600);
  Wire.begin();
  pinMode(PIN_MP3_BUSY, INPUT_PULLUP);
  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);

  Serial.println(F("Урок 5: САМОПЕРЕВІРКА після пайки та збирання в корпус"));
  checkDisplay();
  if (displayOk) {
    showRow(ROW_TITLE, F("SELF-"), "TEST");
    showRow(ROW_OLED, F("OLED"), "OK");
    showRow(ROW_MP3, F("MP3"), "..");
  }
  checkStuckButtons();
  checkPlayer();
  startTest();
}

void loop() {
  readButtons();
  updateTest();

  // Поки все готово, раз на секунду оновлюємо годинник на екрані — видно, що він цокає.
  static unsigned long lastClockDraw = 0;
  if (step == STEP_DONE && resRtc == R_OK && millis() - lastClockDraw >= 1000) {
    lastClockDraw = millis();
    drawRtc();
  }
}
