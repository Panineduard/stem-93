// Навчальний емулятор DFPlayer Mini для Wokwi (custom chip).
//
// У Wokwi немає готової деталі DFPlayer Mini, тому цей чип імітує лише
// ту частину протоколу, яка потрібна уроку 1. Звук він НЕ відтворює:
// «відтворення» видно за піном BUSY (LOW = грає) і за повідомленнями в UART.
//
// Формат кадру DFPlayer (9600 бод, 8N1, 10 байтів):
//   7E FF 06 CMD ACK PARAM_H PARAM_L CHECKSUM_H CHECKSUM_L EF
//   CHECKSUM = -(FF + 06 + CMD + ACK + PARAM_H + PARAM_L)
//
// Підтримані команди: 0x03 play, 0x06 volume, 0x0C reset, 0x0E pause,
// 0x16 stop, 0x42 стан, 0x43 гучність, 0x48 кількість файлів на SD.
// Налаштування (керування чипом у симуляторі): sdCard, trackCount, trackSeconds.

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAME_LEN 10
#define TX_QUEUE_LEN 8

#define CMD_PLAY 0x03
#define CMD_VOLUME 0x06
#define CMD_RESET 0x0C
#define CMD_PAUSE 0x0E
#define CMD_STOP 0x16
#define CMD_QUERY_STATE 0x42
#define CMD_QUERY_VOLUME 0x43
#define CMD_QUERY_SD_FILES 0x48

#define MSG_FINISHED 0x3D
#define MSG_ONLINE 0x3F
#define MSG_ERROR 0x40
#define MSG_ACK 0x41

#define ERR_BUSY 0x0001          // модуль не готовий / немає носія
#define ERR_FILE_INDEX_OUT 0x0005  // немає треку з таким номером

#define BOOT_DELAY_US 1000000     // «завантаження» після reset
#define START_DELAY_US 150000     // затримка до BUSY = LOW після play

typedef struct {
  uart_dev_t uart;
  pin_t busy;

  uint32_t attr_sd_card;
  uint32_t attr_track_count;
  uint32_t attr_track_seconds;

  timer_t boot_timer;
  timer_t start_timer;
  timer_t finish_timer;

  uint8_t rx[FRAME_LEN];
  uint8_t rx_index;

  uint8_t tx_queue[TX_QUEUE_LEN][FRAME_LEN];
  uint8_t tx_head;
  uint8_t tx_count;
  bool tx_busy;

  uint16_t track;
  uint8_t volume;
  bool playing;
} chip_state_t;

static uint16_t checksum(const uint8_t *frame) {
  uint16_t sum = 0;
  for (int i = 1; i <= 6; i++) {
    sum += frame[i];
  }
  return (uint16_t)(-sum);
}

static void tx_kick(chip_state_t *chip) {
  if (chip->tx_busy || chip->tx_count == 0) {
    return;
  }
  chip->tx_busy = uart_write(chip->uart, chip->tx_queue[chip->tx_head], FRAME_LEN);
}

static void on_uart_write_done(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  chip->tx_head = (chip->tx_head + 1) % TX_QUEUE_LEN;
  chip->tx_count--;
  chip->tx_busy = false;
  tx_kick(chip);
}

static void send_frame(chip_state_t *chip, uint8_t cmd, uint16_t param) {
  if (chip->tx_count >= TX_QUEUE_LEN) {
    printf("[DFPlayer] черга UART переповнена, кадр 0x%02X втрачено\n", cmd);
    return;
  }
  uint8_t index = (chip->tx_head + chip->tx_count) % TX_QUEUE_LEN;
  uint8_t *frame = chip->tx_queue[index];
  frame[0] = 0x7E;
  frame[1] = 0xFF;
  frame[2] = 0x06;
  frame[3] = cmd;
  frame[4] = 0x00;
  frame[5] = (uint8_t)(param >> 8);
  frame[6] = (uint8_t)(param & 0xFF);
  uint16_t sum = checksum(frame);
  frame[7] = (uint8_t)(sum >> 8);
  frame[8] = (uint8_t)(sum & 0xFF);
  frame[9] = 0xEF;
  chip->tx_count++;
  tx_kick(chip);
}

static bool sd_inserted(chip_state_t *chip) {
  return attr_read(chip->attr_sd_card) != 0;
}

static void stop_playback(chip_state_t *chip) {
  timer_stop(chip->start_timer);
  timer_stop(chip->finish_timer);
  chip->playing = false;
  pin_write(chip->busy, HIGH);
}

static void on_boot_done(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (sd_inserted(chip)) {
    printf("[DFPlayer] готовий, SD-карта онлайн\n");
    send_frame(chip, MSG_ONLINE, 0x0002);
  } else {
    // Без носія модуль не надсилає «онлайн» — Arduino має побачити таймаут.
    printf("[DFPlayer] SD-карту не знайдено\n");
  }
}

static void on_start(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  chip->playing = true;
  pin_write(chip->busy, LOW);
  uint32_t seconds = attr_read(chip->attr_track_seconds);
  if (seconds == 0) {
    seconds = 1;
  }
  printf("[DFPlayer] ▶ трек %u грає (%u с, гучність %u)\n", chip->track, seconds, chip->volume);
  timer_start(chip->finish_timer, seconds * 1000000, false);
}

static void on_finish(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  chip->playing = false;
  pin_write(chip->busy, HIGH);
  printf("[DFPlayer] ■ трек %u завершився\n", chip->track);
  send_frame(chip, MSG_FINISHED, chip->track);
}

static void handle_command(chip_state_t *chip, uint8_t cmd, bool ack, uint16_t param) {
  bool sd = sd_inserted(chip);
  uint32_t track_count = sd ? attr_read(chip->attr_track_count) : 0;

  if (ack) {
    send_frame(chip, MSG_ACK, 0);
  }

  switch (cmd) {
    case CMD_PLAY:
      if (!sd) {
        printf("[DFPlayer] play %u: немає SD-карти\n", param);
        send_frame(chip, MSG_ERROR, ERR_BUSY);
      } else if (param == 0 || param > track_count) {
        printf("[DFPlayer] play %u: такого треку немає (треків: %u)\n", param, track_count);
        send_frame(chip, MSG_ERROR, ERR_FILE_INDEX_OUT);
      } else {
        stop_playback(chip);
        chip->track = param;
        timer_start(chip->start_timer, START_DELAY_US, false);
      }
      break;

    case CMD_VOLUME:
      chip->volume = param > 30 ? 30 : (uint8_t)param;
      printf("[DFPlayer] гучність %u\n", chip->volume);
      break;

    case CMD_RESET:
      printf("[DFPlayer] reset\n");
      stop_playback(chip);
      timer_start(chip->boot_timer, BOOT_DELAY_US, false);
      break;

    case CMD_PAUSE:
    case CMD_STOP:
      stop_playback(chip);
      printf("[DFPlayer] зупинено\n");
      break;

    case CMD_QUERY_STATE:
      send_frame(chip, CMD_QUERY_STATE, sd ? (uint16_t)(0x0200 | (chip->playing ? 1 : 0)) : 0);
      break;

    case CMD_QUERY_VOLUME:
      send_frame(chip, CMD_QUERY_VOLUME, chip->volume);
      break;

    case CMD_QUERY_SD_FILES:
      if (sd) {
        send_frame(chip, CMD_QUERY_SD_FILES, (uint16_t)track_count);
      } else {
        send_frame(chip, MSG_ERROR, ERR_BUSY);
      }
      break;

    default:
      printf("[DFPlayer] команда 0x%02X не підтримується емулятором\n", cmd);
      break;
  }
}

static void on_uart_rx_data(void *user_data, uint8_t byte) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (chip->rx_index == 0 && byte != 0x7E) {
    return;  // чекаємо початок кадру
  }
  chip->rx[chip->rx_index++] = byte;
  if (chip->rx_index < FRAME_LEN) {
    return;
  }
  chip->rx_index = 0;

  uint8_t *f = chip->rx;
  uint16_t received_sum = ((uint16_t)f[7] << 8) | f[8];
  if (f[1] != 0xFF || f[2] != 0x06 || f[9] != 0xEF || received_sum != checksum(f)) {
    printf("[DFPlayer] пошкоджений кадр — ігнорую\n");
    return;
  }
  handle_command(chip, f[3], f[4] != 0, ((uint16_t)f[5] << 8) | f[6]);
}

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  memset(chip, 0, sizeof(chip_state_t));

  chip->attr_sd_card = attr_init("sdCard", 1);
  chip->attr_track_count = attr_init("trackCount", 3);
  chip->attr_track_seconds = attr_init("trackSeconds", 5);
  chip->volume = 15;

  chip->busy = pin_init("BUSY", OUTPUT_HIGH);

  const uart_config_t uart_config = {
    .tx = pin_init("TX", INPUT_PULLUP),
    .rx = pin_init("RX", INPUT),
    .baud_rate = 9600,
    .rx_data = on_uart_rx_data,
    .write_done = on_uart_write_done,
    .user_data = chip,
  };
  chip->uart = uart_init(&uart_config);

  const timer_config_t boot_config = { .callback = on_boot_done, .user_data = chip };
  const timer_config_t start_config = { .callback = on_start, .user_data = chip };
  const timer_config_t finish_config = { .callback = on_finish, .user_data = chip };
  chip->boot_timer = timer_init(&boot_config);
  chip->start_timer = timer_init(&start_config);
  chip->finish_timer = timer_init(&finish_config);

  // Справжній модуль після ввімкнення сам повідомляє, що носій онлайн.
  timer_start(chip->boot_timer, BOOT_DELAY_US, false);
  printf("[DFPlayer] емулятор запущено\n");
}
