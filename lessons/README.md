# Уроки STEM-93: Smart School Bell

Кожен урок — окрема папка з планом (`README.md`), а коли є код — зі скетчем і схемою для Wokwi. Повний план курсу: [`dev/lessons.md`](../dev/lessons.md).

Основна плата: **Arduino Uno**. MP3-модуль: **DFPlayer Mini**.

| # | Урок | Mission | Код |
| --- | --- | --- | --- |
| 1 | [Make it speak 🔊](lesson-01-make-it-speak/) | Uno + DFPlayer Mini + динамік → перший звук | ✅ скетч + Wokwi |
| 2 | [Make it know the time ⏰](lesson-02-know-the-time/) | Пристрій знає поточний час (RTC) | ⏳ RTC не обрано |
| 3 | [Make it automatic ⚙️](lesson-03-make-it-automatic/) | TIME + AUDIO: трек за розкладом | ⏳ |
| 4 | [Керуємо підсвічувачем 💡](lesson-04-control-light/) | Навантаження вмикається разом із треком | ⏳ драйвер не обрано |
| 5 | [Display / UI 🖥️](lesson-05-display-ui/) | Пристрій показує стан | ⏳ дисплей не обрано |
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
