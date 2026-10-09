// Урок 1 — Make it speak 🔊
// Arduino Uno + DFPlayer Mini: відтворити перший трек із microSD.
//
// Схема (див. ../README.md та ../wokwi/diagram.json):
//   DFPlayer VCC  → Uno 5V
//   DFPlayer GND  → Uno GND
//   DFPlayer TX   → Uno D10 (SoftwareSerial RX)
//   DFPlayer RX   ← резистор 1 кОм ← Uno D11 (SoftwareSerial TX)
//   DFPlayer BUSY → Uno D7  (LOW = трек грає)
//   DFPlayer SPK_1/SPK_2 → динамік 4–8 Ом, до 3 Вт
//
// Бібліотека: DFRobotDFPlayerMini (Arduino Library Manager).
//
// Важливо: «Грає» показуємо лише тоді, коли модуль підтвердив відтворення
// сигналом BUSY = LOW. Сама команда play() ще нічого не гарантує.

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// --- Піни (обрані для уроку 1, Arduino Uno) ---
const uint8_t PIN_MP3_RX = 10;   // Uno приймає ← TX DFPlayer
const uint8_t PIN_MP3_TX = 11;   // Uno передає → 1 кОм → RX DFPlayer
const uint8_t PIN_MP3_BUSY = 7;  // LOW, поки трек грає

// --- Налаштування ---
const uint16_t TRACK_NUMBER = 1;  // перший файл на SD-карті
const uint8_t VOLUME = 20;        // 0..30
const unsigned long PLAY_CONFIRM_TIMEOUT_MS = 1500;  // скільки чекаємо BUSY = LOW

SoftwareSerial mp3Serial(PIN_MP3_RX, PIN_MP3_TX);
DFRobotDFPlayerMini player;

// Стани плеєра
enum PlayerState {
  STATE_NO_PLAYER,  // модуль не відповів або немає SD-карти
  STATE_READY,      // готовий, нічого не грає
  STATE_STARTING,   // команду play надіслано, чекаємо підтвердження
  STATE_PLAYING     // BUSY = LOW, звук підтверджено
};

PlayerState state = STATE_NO_PLAYER;
unsigned long startRequestedAt = 0;

bool isBusyLow() {
  return digitalRead(PIN_MP3_BUSY) == LOW;
}

// Надсилає команду відтворення. Результат перевіряємо в loop().
void startTrack() {
  Serial.print(F("Команда: відтворити трек "));
  Serial.println(TRACK_NUMBER);
  player.play(TRACK_NUMBER);
  startRequestedAt = millis();
  state = STATE_STARTING;
}

// Друкує повідомлення, які DFPlayer сам надсилає в Arduino.
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

void setup() {
  Serial.begin(9600);
  mp3Serial.begin(9600);

  pinMode(PIN_MP3_BUSY, INPUT_PULLUP);

  Serial.println(F("Урок 1: Arduino Uno + DFPlayer Mini"));
  Serial.println(F("Запуск DFPlayer..."));

  // begin() скидає модуль і чекає до ~2 с на відповідь «SD-карта готова».
  // Це блокує лише один раз під час старту.
  if (!player.begin(mp3Serial, true, true)) {
    Serial.println(F("ПОМИЛКА: DFPlayer не відповідає або немає SD-карти."));
    Serial.println(F("Перевірте дроти TX/RX, живлення 5V, GND і SD-карту, потім натисніть RESET."));
    state = STATE_NO_PLAYER;
    return;
  }

  Serial.println(F("DFPlayer готовий."));

  // Кількість файлів питаємо одразу після begin(): якщо перед цим надіслати
  // іншу команду з ACK (напр., volume), бібліотека поверне застарілий TimeOut.
  int fileCount = player.readFileCounts();
  if (fileCount < 0) {
    Serial.println(F("Увага: не вдалося прочитати кількість файлів на SD."));
  } else {
    Serial.print(F("Файлів на SD: "));
    Serial.println(fileCount);
    if (fileCount < (int)TRACK_NUMBER) {
      Serial.println(F("ПОМИЛКА: на SD-карті немає треку з таким номером."));
      state = STATE_READY;
      return;
    }
  }

  player.volume(VOLUME);
  state = STATE_READY;
  startTrack();
}

void loop() {
  // 1. Повідомлення від DFPlayer (не блокує: читає лише те, що вже прийшло).
  if (state != STATE_NO_PLAYER && player.available()) {
    printPlayerMessage(player.readType(), player.read());
  }

  // 2. Стан плеєра за сигналом BUSY.
  switch (state) {
    case STATE_STARTING:
      if (isBusyLow()) {
        state = STATE_PLAYING;
        Serial.println(F("ГРАЄ: відтворення підтверджено (BUSY = LOW)."));
      } else if (millis() - startRequestedAt >= PLAY_CONFIRM_TIMEOUT_MS) {
        state = STATE_READY;
        Serial.println(F("ПОМИЛКА: трек не запустився (немає підтвердження BUSY)."));
      }
      break;

    case STATE_PLAYING:
      if (!isBusyLow()) {
        state = STATE_READY;
        Serial.println(F("ГОТОВО: трек більше не грає."));
      }
      break;

    default:
      break;
  }
}
