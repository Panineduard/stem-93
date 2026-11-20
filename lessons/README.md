# Уроки STEM-93: Smart School Bell

Кожен урок — окрема папка з планом (`README.md`), тематичною сторінкою-презентацією (`presentation.html`), а коли є код — зі скетчем (уроки 1–4 ще й з Wokwi). Повний план курсу: [`dev/lessons.md`](../dev/lessons.md).

Основна плата: **Arduino Uno**. MP3-модуль: **DFPlayer Mini**. Годинник: **DS3231** (з уроку 2). Дисплей: **OLED SSD1306 128×64, I²C** і **дві кнопки** на D2/D3 (з уроку 3). Реле: **модуль 5 В, 1 канал** на D4 (з уроку 4, модель ще не обрано).

| # | Урок | Mission | Код |
| --- | --- | --- | --- |
| 1 | [Make it speak 🔊](lesson-01-make-it-speak/) | Uno + DFPlayer Mini + динамік → перший звук | ✅ скетч + Wokwi |
| 2 | [Make it know the time ⏰](lesson-02-know-the-time/) | DS3231 + шкільний розклад в EEPROM | ✅ скетч + Wokwi |
| 3 | [Make it show & listen 👀](lesson-03-show-and-set/) | OLED показує час і розклад; меню з двох кнопок змінює розклад без перепрошивки | ✅ скетч + Wokwi |
| 4 | [Розбуди підсилювач ⚡](lesson-04-wake-the-amp/) | Безпека струму; реле на D4 вмикає підсилювач за хвилину до події і вимикає після | ✅ скетч + Wokwi (модель реле не обрано) |
| 5 | [Збери в корпус 🧱](lesson-05-box-it-up/) | Пайка, отвори, кріплення; самоперевірка кожного модуля після збирання; Arduino Challenge | ✅ скетч самоперевірки (компілюється) + презентація |
| 6 | [Integration & QA 🧪](lesson-06-integration-qa/) | Усе разом, тести, Final Demo | ⏳ |

## Структура уроку з кодом

```text
lesson-01-make-it-speak/
├── README.md              # план уроку, схема, Definition of Done
├── sketch/sketch.ino      # скетч для Arduino IDE / arduino-cli
└── wokwi/                 # симуляція
    ├── diagram.json
    ├── libraries.txt
    ├── wokwi.toml         # для Wokwi CLI / MCP / VS Code
    ├── dfplayer-mini.chip.json
    └── dfplayer-mini.chip.c   # емулятор DFPlayer Mini (custom chip)
```

Скетч і файли Wokwi лежать у різних папках, бо Arduino IDE намагається компілювати всі `.c` файли поруч зі скетчем.
