---
# Онлайн-версія презентації: https://claude.ai/artifact/WxFhBJyxJdCkDkVNuMcwxm
marp: true
lang: uk
title: "Урок 1 — Змусь машину говорити"
paginate: true
footer: "STEM-93 · Рівень 01 · Аудіолаб"
style: |
  /* Стиль «ризограф-комікс»: папір, товсті контури, зсунуті тіні, неонові плашки */
  @import url('https://fonts.googleapis.com/css2?family=Unbounded:wght@600;800;900&family=Golos+Text:wght@400;600;800&family=JetBrains+Mono:wght@600&display=swap');

  :root {
    --ink: #16141F;
    --paper: #FFF4DF;
    --pink: #FF4FA3;
    --blue: #2F5BFF;
    --yellow: #FFD60A;
    --mint: #2EE6A6;
    --white: #FFFFFF;
  }

  section {
    background-color: var(--paper);
    background-image: radial-gradient(rgba(22, 20, 31, .09) 1.6px, transparent 1.7px);
    background-size: 22px 22px;
    color: var(--ink);
    font-family: 'Golos Text', sans-serif;
    font-size: 28px;
    line-height: 1.35;
    padding: 60px 80px 90px;
  }

  h1, h2, h3, h6 { font-family: 'Unbounded', sans-serif; color: var(--ink); }
  h1 { font-size: 60px; font-weight: 900; line-height: 1.05; letter-spacing: -0.02em; margin: 6px 0 22px; }
  h2 { font-size: 42px; font-weight: 800; margin: 0 0 18px; }
  h3 {
    display: inline-block; font-size: 26px; font-weight: 800;
    background: var(--yellow); border: 3px solid var(--ink); border-radius: 14px;
    padding: 10px 20px; box-shadow: 6px 6px 0 var(--ink); transform: rotate(-1.2deg);
    margin-top: 18px;
  }
  /* h6 — наліпка-мітка над заголовком */
  h6 {
    display: inline-block; font-size: 16px; font-weight: 800;
    letter-spacing: .14em; text-transform: uppercase;
    background: var(--ink); color: var(--paper);
    padding: 8px 18px; border-radius: 999px; transform: rotate(-2deg); margin: 0 0 6px;
  }

  strong { font-weight: 800; background: linear-gradient(transparent 58%, var(--yellow) 58%); padding: 0 2px; }
  code {
    font-family: 'JetBrains Mono', monospace; font-size: .88em;
    background: var(--ink); color: var(--mint); padding: 2px 10px; border-radius: 8px;
  }

  /* blockquote — бульбашка з реплікою, як у коміксі */
  blockquote {
    position: relative; margin: 26px 0 30px; padding: 18px 28px;
    background: var(--white); color: var(--ink);
    border: 4px solid var(--ink); border-radius: 28px; box-shadow: 8px 8px 0 var(--ink);
  }
  blockquote::before { content: none; }
  blockquote::after {
    content: ""; position: absolute; left: 64px; bottom: -17px; width: 26px; height: 26px;
    background: var(--white); border-right: 4px solid var(--ink); border-bottom: 4px solid var(--ink);
    transform: rotate(45deg);
  }

  ul { padding-left: 1.1em; }
  ul > li::marker { content: "◆  "; color: var(--pink); }
  li { margin: 6px 0; }

  /* нумеровані кроки — кружечки з номером */
  ol { list-style: none; counter-reset: step; padding-left: 0; }
  ol > li { counter-increment: step; position: relative; padding-left: 62px; margin: 12px 0; }
  ol > li::before {
    content: counter(step); position: absolute; left: 0; top: -4px;
    width: 42px; height: 42px; display: flex; align-items: center; justify-content: center;
    font-family: 'Unbounded', sans-serif; font-weight: 800; font-size: 20px;
    background: var(--pink); border: 3px solid var(--ink); border-radius: 50%;
    box-shadow: 3px 3px 0 var(--ink);
  }

  table {
    border-collapse: separate; border-spacing: 0; font-size: 25px;
    border: 4px solid var(--ink); border-radius: 18px; overflow: hidden;
    box-shadow: 8px 8px 0 var(--ink); background: var(--white);
  }
  th {
    font-family: 'Unbounded', sans-serif; font-size: 18px; text-transform: uppercase; letter-spacing: .06em;
    background: var(--pink); color: var(--ink); border: 0; border-bottom: 4px solid var(--ink); padding: 14px 24px;
  }
  td { background: var(--white); border: 0; border-bottom: 2px dashed rgba(22, 20, 31, .25); padding: 14px 24px; }
  tr:last-child td { border-bottom: 0; }
  table tr:nth-child(2n) td { background: #FFF9EC; }

  footer {
    font-family: 'Unbounded', sans-serif; font-size: 13px; font-weight: 600;
    letter-spacing: .14em; text-transform: uppercase; color: var(--ink); opacity: .6;
    left: 80px; bottom: 34px;
  }
  /* номер слайда — жовтий значок */
  section::after {
    font-family: 'Unbounded', sans-serif; font-weight: 800; font-size: 18px; color: var(--ink);
    width: 52px; height: 52px; padding: 0; right: 40px; bottom: 26px;
    display: flex; align-items: center; justify-content: center;
    background: var(--yellow); border: 3px solid var(--ink); border-radius: 50%;
    box-shadow: 4px 4px 0 var(--ink);
  }

  /* ---------- ОБКЛАДИНКА ---------- */
  section.cover {
    background-color: var(--pink);
    background-image: radial-gradient(rgba(22, 20, 31, .2) 2px, transparent 2.2px);
    background-size: 18px 18px;
    justify-content: center;
  }
  section.cover h1 {
    font-size: 100px; color: var(--paper); max-width: 820px;
    -webkit-text-stroke: 3px var(--ink); text-shadow: 8px 8px 0 var(--ink);
  }
  section.cover h6 { background: var(--yellow); color: var(--ink); border: 3px solid var(--ink); }
  section.cover p { font-size: 32px; font-weight: 600; max-width: 760px; }
  section.cover strong { background: var(--paper); border: 3px solid var(--ink); border-radius: 10px; padding: 0 10px; }
  /* «еквалайзер» праворуч */
  section.cover::before {
    content: ""; position: absolute; right: 90px; bottom: 120px; width: 270px; height: 300px;
    background:
      linear-gradient(var(--ink), var(--ink)) 0px 100% / 22px 35% no-repeat,
      linear-gradient(var(--yellow), var(--yellow)) 36px 100% / 22px 70% no-repeat,
      linear-gradient(var(--ink), var(--ink)) 72px 100% / 22px 100% no-repeat,
      linear-gradient(var(--paper), var(--paper)) 108px 100% / 22px 55% no-repeat,
      linear-gradient(var(--ink), var(--ink)) 144px 100% / 22px 85% no-repeat,
      linear-gradient(var(--yellow), var(--yellow)) 180px 100% / 22px 45% no-repeat,
      linear-gradient(var(--ink), var(--ink)) 216px 100% / 22px 65% no-repeat,
      linear-gradient(var(--paper), var(--paper)) 252px 100% / 18px 25% no-repeat;
  }

  /* ---------- ПИТАННЯ / ЗАВДАННЯ ---------- */
  section.quest { background-color: var(--yellow); justify-content: center; }
  section.quest h1 { font-size: 78px; max-width: 1050px; }
  section.quest strong { background: linear-gradient(transparent 58%, var(--pink) 58%); }
  section.quest::after { background: var(--paper); }

  /* ---------- ТЕОРІЯ ---------- */
  section.theory {
    background-color: var(--blue);
    background-image:
      linear-gradient(rgba(255, 255, 255, .08) 2px, transparent 2px),
      linear-gradient(90deg, rgba(255, 255, 255, .08) 2px, transparent 2px);
    background-size: 44px 44px;
    color: var(--paper);
  }
  section.theory h1, section.theory h2 { color: var(--paper); text-shadow: 5px 5px 0 var(--ink); }
  section.theory h6 { background: var(--yellow); color: var(--ink); }
  section.theory strong { background: linear-gradient(transparent 58%, var(--pink) 58%); color: var(--white); }
  section.theory ul > li::marker { color: var(--yellow); }
  section.theory ol > li::before { background: var(--yellow); color: var(--ink); }
  section.theory footer { color: var(--paper); }
  section.theory blockquote strong { color: var(--ink); }

  /* ---------- ЛАНЦЮГ «ВІД КОМАНДИ ДО ЗВУКУ» ---------- */
  section.chain ol { display: flex; gap: 46px; margin: 30px 0 34px; }
  section.chain ol > li {
    flex: 1; margin: 0; padding: 22px 20px; font-size: 22px; line-height: 1.3;
    border: 4px solid var(--ink); border-radius: 22px; box-shadow: 7px 7px 0 var(--ink);
    background: var(--white);
  }
  section.chain ol > li:nth-child(1) { background: var(--yellow); transform: rotate(-1.5deg); }
  section.chain ol > li:nth-child(2) { background: var(--pink); transform: rotate(1deg); }
  section.chain ol > li:nth-child(3) { background: var(--mint); transform: rotate(-1deg); }
  section.chain ol > li:nth-child(4) { background: var(--ink); color: var(--paper); transform: rotate(1.5deg); }
  section.chain ol > li::before { content: none; }
  section.chain ol > li:not(:last-child)::after {
    content: "➜"; position: absolute; right: -42px; top: 50%; transform: translateY(-50%);
    font-size: 34px; color: var(--ink);
  }
  section.chain ol > li strong {
    display: block; background: none; padding: 0; margin-bottom: 8px;
    font-family: 'Unbounded', sans-serif; font-size: 22px;
  }
  section.chain ol > li:nth-child(4) strong { color: var(--yellow); }

  /* ---------- КАРТКИ КОМАНДИ ---------- */
  section.cards ul { list-style: none; padding: 0; display: grid; grid-template-columns: 1fr 1fr; gap: 22px 30px; margin: 10px 0 14px; }
  section.cards ul > li::marker { content: none; }
  section.cards ul > li {
    margin: 0; padding: 16px 22px; font-size: 23px;
    background: var(--white); border: 4px solid var(--ink); border-radius: 20px; box-shadow: 6px 6px 0 var(--ink);
  }
  section.cards ul > li:nth-child(1) { border-top: 16px solid var(--yellow); }
  section.cards ul > li:nth-child(2) { border-top: 16px solid var(--pink); }
  section.cards ul > li:nth-child(3) { border-top: 16px solid var(--mint); }
  section.cards ul > li:nth-child(4) { border-top: 16px solid var(--blue); }
  section.cards ul > li strong { display: block; background: none; padding: 0; font-family: 'Unbounded', sans-serif; font-size: 22px; }
  section.cards blockquote { margin-top: 30px; }

  /* ---------- ЛАБОРАТОРІЯ (практика) ---------- */
  section.lab { font-size: 25px; }
  section.lab h1 { font-size: 50px; }

  /* ---------- НАЛАГОДЖЕННЯ: смуга «обережно» зверху ---------- */
  section.alert::before {
    content: ""; position: absolute; top: 0; left: 0; right: 0; height: 24px;
    background: repeating-linear-gradient(45deg, var(--yellow) 0 26px, var(--ink) 26px 52px);
  }
  section.alert table { font-size: 22px; }
  section.alert td, section.alert th { padding: 11px 20px; }

  /* ---------- ПЕРЕМОГА ---------- */
  section.win {
    background-color: var(--ink); color: var(--paper); justify-content: center; text-align: center;
    background-image:
      radial-gradient(circle, rgba(255, 79, 163, .85) 6px, transparent 7px),
      radial-gradient(circle, rgba(255, 214, 10, .8) 5px, transparent 6px),
      radial-gradient(circle, rgba(46, 230, 166, .7) 4px, transparent 5px);
    background-size: 150px 150px, 115px 115px, 90px 90px;
    background-position: 0 0, 50px 70px, 75px 25px;
  }
  section.win h1 {
    font-size: 84px; color: var(--yellow); margin: 18px auto 26px; max-width: 1000px;
    text-shadow: 6px 6px 0 var(--pink);
  }
  section.win h6 { background: var(--mint); color: var(--ink); font-size: 20px; }
  section.win p {
    display: inline-block; font-size: 30px; padding: 16px 26px; max-width: 980px; margin: 0 auto;
    background: var(--ink); border: 3px solid var(--mint); border-radius: 18px;
  }
  section.win strong { color: var(--ink); background: var(--yellow); border-radius: 8px; padding: 0 10px; }
  section.win footer { color: var(--paper); }

  /* ---------- НАСТУПНІ РІВНІ ---------- */
  section.levels h1 { font-size: 50px; }
  section.levels ul { list-style: none; padding: 0; margin: 4px 0 16px; }
  section.levels ul > li::marker { content: none; }
  section.levels ul > li {
    margin: 10px 0; padding: 7px 26px; font-size: 23px; max-width: 1000px;
    background: var(--white); border: 3px solid var(--ink); border-radius: 999px; box-shadow: 5px 5px 0 var(--ink);
  }
  section.levels ul > li:nth-child(odd) { transform: translateX(26px); }

  /* ---------- ВИХІДНИЙ КВИТОК: пунктирна рамка ---------- */
  section.ticket::before {
    content: ""; position: absolute; top: 26px; left: 26px; right: 26px; bottom: 26px; pointer-events: none;
    border: 4px dashed var(--ink); border-radius: 30px;
  }
  section.ticket h1 { font-size: 50px; }
  section.ticket h3 { background: var(--mint); }
---

<!-- _class: cover -->
<!-- _paginate: false -->
<!-- _footer: "" -->

###### Рівень 01 · Аудіолаб STEM-93

# Змусь машину говорити

**Місія:** Arduino дає команду — ми чуємо перший звук. 🔊

<!--
Нотатки викладача: відкрийте урок із питання, а не з теорії. Перший звук — головна мета всього заняття.
-->

---

<!-- _class: quest -->

###### Розминка · 3 хвилини

# Що потрібно машині, щоб видати звук?

> Намалюйте шлях **від команди до ваших вух**. Помилятися можна — порівняємо версії за хвилину.

<!--
Зберіть 2–3 малюнки й залиште їх на дошці: до них повернемося на слайді з ланцюгом.
-->

---

<!-- _class: chain -->

###### Сьогоднішній ланцюг

# Від команди до звуку

1. **Arduino Uno** надсилає команду «грай трек 1»
2. **DFPlayer Mini** знаходить MP3 на microSD і декодує
3. **Динамік** перетворює сигнал на коливання повітря
4. **Звук** 🎉 доказ, що все спрацювало

Під час увімкнення Arduino Uno автоматично запускає **трек №1**. Розклад і точний час будуть на наступних рівнях.

---

<!-- _class: cards -->

###### Знайомтеся: наша команда

# Хто за що відповідає?

- **Arduino Uno** мозок: виконує програму й надсилає команди.
- **DFPlayer Mini** DJ: шукає трек, декодує MP3, видає аудіосигнал.
- **microSD** фонотека: зберігає аудіофайли.
- **Динамік** голос: перетворює сигнал на звук.

> 🤔 Чи надсилає Arduino саму музику по UART?

---

<!-- _class: theory -->

###### Теорія · 1

# UART — розмова двох плат

- Дані йдуть **послідовно**, біт за бітом.
- Лінії перехресні: **TX** одного → **RX** іншого.
- У нашому скетчі Arduino слухає на **D10** (RX), а говорить через **D11** (TX).
- Обидві плати домовляються про швидкість: `9600` бод.

> UART везе **команди й відповіді**, а не музику. Звук іде окремим шляхом: з аудіовиходу DFPlayer до динаміка.

---

<!-- _class: theory -->

###### Теорія · 2

# `play(1)` — це прохання, а не доказ

Команда лише **просить** запустити трек. Чи він справді грає, перевіряємо трьома рівнями доказу:

1. модуль **відповів** на запуск;
2. лінія **BUSY → D7** стала `LOW` (трек грає);
3. ми **чуємо** звук у динаміку.

### 🕵️ Звуку немає. Що перевіриш першим?

---

<!-- _class: lab -->

![bg right:40% contain](wiring.svg)

###### Практика · збирання

# Збираємо без поспіху

1. Перепідключаємо тільки **знеструмлену** схему.
2. Звіряємо кожен контакт зі схемою уроку.
3. Спільний **GND**, а TX/RX навхрест.
4. Перед живленням напарник перевіряє стенд.
5. Сумніваєшся в компоненті чи напрузі? **Стоп**, кличемо викладача.

---

<!-- _class: lab -->

###### Детектив коду

# Прочитай програму до запуску

Знайди у `sketch/sketch.ino`:

- де задані контакти `D10`, `D11`, `D7`;
- де стартує зв'язок із DFPlayer;
- де надсилається команда відтворення;
- де код перевіряє BUSY і повідомляє про помилку.

### 🔮 Передбач: що покаже Serial Monitor?

---

<!-- _class: lab -->

###### Експеримент

# Запускаємо й записуємо факти

| Джерело | Що перевіряємо | Що побачили? |
| --- | --- | --- |
| Serial Monitor | модуль, файл, повідомлення | |
| BUSY (D7) | `LOW` під час відтворення? | |
| Динамік | звук чути? | |

> Скетч завантажився ≠ система **довела**, що звук відтворюється.

---

<!-- _class: alert -->

###### Режим налагодження

# Щось мовчить? Шукаємо причину

| Симптом | Перевір |
| --- | --- |
| Модуль не відповідає | живлення, GND, TX/RX, швидкість |
| Трек не знайдено | microSD, файл, номер треку |
| BUSY не активний | контакт BUSY, результат запуску |
| BUSY активний, а тиша | аудіовихід, дроти динаміка, гучність |

**Правило:** одна можлива причина за раз. Wokwi перевіряє логіку, але справжнього звуку не видає.

---

<!-- _class: win -->
<!-- _paginate: false -->

###### 🏆 Досягнення розблоковано

# Ми змусили машину говорити

Тепер кожна команда пояснює: **який доказ показує, що вона справді говорить?**

---

<!-- _class: levels -->

###### Наступні рівні 🔒

# Чого ще бракує шкільному плеєру?

- ⏰ Як пристрій дізнається час і не забуває його після вимкнення?
- ⚙️ Як запустити потрібний трек рівно один раз за розкладом?
- 💡 Чим безпечно керувати шкільним навантаженням?
- 🖥️ Що має бачити людина на екрані?
- 🧪 Як система повідомляє про помилку й відновлюється?

Це **відкриті рішення**. Розкриваємо їх на наступних уроках.

---

<!-- _class: ticket -->

###### Вихідний квиток 🎟️

# Три питання перед виходом

1. Які дані йдуть через UART? А звідки береться звук?
2. Чому після `play(1)` ще рано писати «ГРАЄ»?
3. Яку одну проблему треба вирішити, щоб запуск був за розкладом?

### 🏠 Вдома: намалюй шлях час → рішення → аудіо

Пунктиром познач усе, що ще не обрано або не реалізовано.
