# STEM Project, Episode 4: My Electrician Diploma Finally Paid Off

**Status:** working draft for review; not approved or published. The article is in English, as requested in the draft brief. The Ukrainian notes at the end are for the author, not for publication.

**Interactive angle:** readers get two short “pause and predict” moments before the explanation. The city riddle is the opening game; the technical interaction asks readers to diagnose a relay state and then identify a fail-safe requirement.

---

## Article draft

### STEM Project, Episode 4: My Electrician Diploma Finally Paid Off

**Cover image:** `img/linkedin/00-cover.png` (1920×1080): the relay-test photo in a frame of Iznik tiles, with cats, a tulip glass of tea, and a bridge between two shores. The city is not named.

We powered up the controller.

The relay clicked before the schedule asked it to.

**Pause here: what would you check first?**

A. The clock  
B. The relay’s active HIGH/LOW setting  
C. The audio file

Keep your answer in mind. We’ll come back to it.

After a one-week break, our school audio project returned to a very physical question: how can a small controller prepare an amplifier before a scheduled announcement, then turn it off after the sound is over?

The answer involved one new component, one unexpected click, and a useful debugging lesson.

### First, a small game

Each lesson has a secret city theme. This time the slides offered cats, tulip tea glasses and a city divided by water.

Can you guess the city from the opening image?

**[Add image: `img/linkedin/02-guess-the-city.png`]**

Leave one guess in the comments. We’ll reveal the answer after the article has had a day or two to travel.

Now, back to the relay.

### Safety detective: what gets switched today?

Quick challenge before the relay clicks. In our classroom test, what does it switch?

**A.** The school amplifier  
**B.** A desk lamp from the wall socket  
**C.** Nothing at all

Pick your answer before scrolling.

**It’s C: nothing.** During the test, nothing was connected to the relay contacts, and no 220 V came anywhere near the relay. The module has its own small indicator LED that lights up when the coil switches on. Together with the click you can hear, that LED was all the proof we needed that the logic works. Any future mains installation is for a qualified adult using a suitable, enclosed and checked setup. A relay is a switch; its presence alone does not make every load or wiring arrangement safe.

Two rules for the bench: keep hands dry, and switch power off before changing wires. “Low voltage” is not permission to touch live wiring. Shock risk depends on the current through the body, its path and duration, and the conditions of contact. [UK Health and Safety Executive: Electrical injuries](https://www.hse.gov.uk/electricity/injuries.htm)

**[Visual idea: a close-up of the relay module with its built-in indicator LED lit and nothing connected to the contacts. Don’t use `img/linkedin/03-safety-220v.png` as-is: its body-current calculation oversimplifies the risk.]**

### The relay: a switch moved by a magnet

Denys explained the relay with a chalk drawing: a coil, a moving contact and two separate circuit paths. A signal energizes the coil; the magnetic force moves the contact; the relay makes a click. The Arduino controls the relay module, and the contacts switch the separate load circuit.

That separation is a feature of the relay, not a guarantee that any relay board is safe for any mains load. Our module’s exact contact rating is not confirmed for controlling the school amplifier, so during the classroom test the contacts stayed unconnected.

**[Add photo: `img/linkedin/04-denys-at-the-board.jpg` — Denys explaining at the board.]**  
**[Add photo/diagram: `img/linkedin/05-relay-chalk-drawing.jpg` or `img/linkedin/06-relay-model.png` — relay drawing/model.]**

### Debugging the click

Back to the question: the first thing to check was the relay’s active level.

Relay modules do not all turn on with the same signal. Some activate on `HIGH`; the module used for this classroom build activates on `LOW`. The Wokwi relay model behaves differently and activates on `HIGH`. The sketch therefore has an explicit `RELAY_ACTIVE_LOW` setting instead of assuming every module is wired the same way.

On the first test, the relay came on at startup. We corrected the active-level configuration so the safe default after reset is **relay off**. During the lesson nothing was connected to the contacts: the module’s built-in indicator LED and the click showed the relay state without switching any load.

**[Add close-up/video: the module’s indicator LED switching on with a click. Caption: “No load connected, just the relay’s own LED.”]**

### Why the relay wakes up early

The project is being designed for a daily school moment of silence and scheduled audio. An amplifier may need time to start before sound begins, so the controller’s plan is:

- switch the relay on up to 60 seconds before a scheduled event;
- keep it on across adjacent tracks, without clicking off in between;
- after playback ends, wait for 15 seconds of silence, then switch it off.

For a manual play request, the sketch also allows a 3-second warm-up. It uses time checks rather than a blocking delay, so the buttons and display can remain responsive while it waits.

**Pause and predict:** if two tracks are scheduled one minute apart, when should the relay turn off?

**Answer:** not between the tracks. It should remain on until the final track has ended and the 15-second quiet period has passed.

That is a small piece of logic with a human purpose: avoid cutting off the start of an announcement, and avoid an unnecessary relay click in the middle of the sequence.

### A project team, not a row of job titles

The lesson also made space for students to teach one another. Denys prepared the relay explanation; Evelina compared Scrum, Kanban and Waterfall; Oleksa looked for bugs; Daryna worked on the logo and recorded quote audio; and Anna’s topic—variables, data types and memory—gave the team a useful constraint to keep in mind.

The best story to add here is one real moment from the room: a question, a disagreement, a test that failed, or an explanation that made the idea click.

**[Author: add one specific, observed moment from the lesson. Avoid turning planned roles into claims about what each student completed unless confirmed.]**

**[Optional photo: `img/linkedin/08-evelina-at-the-board.jpg` — Evelina at the board. Confirm consent before publishing identifiable student photos.]**

### The enclosure is taking shape

Away from the relay test, a pink Ukraine-shaped enclosure was printed for the project. The lid has a screen window, two button openings and the STEM-93 logo. The enclosure is a tangible milestone; the components still need to be fitted and the remaining integration work documented accurately.

**[Add photo: `img/linkedin/10-lid-stem93-logo.jpg` — the printed lid.]**

### Your turn: name the fail-safe

The team’s intended behavior after a reset is simple: **the relay starts off**. That gives the project a safe default before the clock, schedule and other parts are ready.

What other “must be true” rule would you add to an unattended school device? For example: “Never claim audio is playing until playback is confirmed.”

Share one rule in the comments—and, if you guessed the city, include that too.

---

## Optional companion format: an interactive LinkedIn document/carousel

Use the article as the full story and make the carousel a “predict, then reveal” companion:

1. **Cover:** “The relay clicked before the schedule. What would you check?”
2. **Choose:** clock / active level / audio file.
3. **Reveal:** the classroom module is active LOW; the Wokwi relay is active HIGH.
4. **Trace:** Arduino signal → relay coil → the module’s indicator LED and the click (contacts unconnected).
5. **Schedule puzzle:** two tracks one minute apart; ask when the relay should turn off.
6. **Reveal:** after the last track and 15 seconds of silence.
7. **Safety boundary:** during the test the relay switched no load; only its own LED showed the state, and no 220 V reached the relay.
8. **Open question:** “What fail-safe should every unattended device have?”

This gives readers two concrete decisions to make rather than asking them to comment just to boost engagement.

---

## Редакторська перевірка перед публікацією

- **Безпека:** видалено твердження, що 5 В безпечно торкатися мокрими руками, і розрахунок “220 В / 1 кОм = смертельний струм”. Наслідки ураження залежать від кількох умов; не варто давати спрощену модель як гарантію безпеки.
- **Навантаження реле (підтверджено автором):** під час перевірки до контактів реле нічого не підключали, 220 В до реле не підводили. Окремого світлодіода не було: стан реле показував вбудований світлодіод модуля і клацання. Не писати, що реле вмикало лампу, світлодіод 5 В чи підсилювач.
- **Межі тверджень:** реле керує своїми контактами; воно саме по собі не підтверджує, що підсилювач увімкнувся. Модель і допустиме навантаження контактів ще не підтверджені.
- **Полярність:** скетч зараз налаштований на активний LOW для класного модуля; модель реле Wokwi активується HIGH. Це корисна інтерактивна загадка, але назви симулятор і реальний модуль окремо.
- **Пам’ять:** прибрано твердження про 96% флеш-пам’яті. У README є інші результати компіляції/версії скетчу; число додавати тільки з актуального фактичного логу збірки.
- **Команда:** перевірити правильність імен, ролей та завершених завдань. У статті ролі описані як внески/теми, а не як результати без підтвердження.
- **Наступний урок:** попередній анонс про складання корпуса, пайку й блок живлення прибрано, бо це треба звірити з актуальним планом уроку 5 перед публікацією.
- **Місто:** загадка з’являється на початку; прибрано подальше порівняння міста з принципом реле, якого не було в брифі.
- **Зображення і дозволи:** перевірити, що кожен файл існує, підпис відповідає зображенню, а для впізнаваних учнів є дозвіл на публікацію. Відносні шляхи в цьому файлі — робочі посилання для підготовки; LinkedIn вимагатиме завантажити зображення окремо.
- **Відповідь на загадку:** після публікації повернутися з назвою міста в коментар або додати її пізніше в статтю.

**Джерело для електробезпеки:** [UK Health and Safety Executive — Electrical injuries](https://www.hse.gov.uk/electricity/injuries.htm).
