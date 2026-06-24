// Wokwi Custom SPI Echo Chip
//
// Echoes back in the CURRENT CS transaction exactly the bytes that were
// received in the PREVIOUS CS transaction, byte-for-byte with no transformation.
// On the very first transaction the chip sends all-zero bytes.
//
// This design makes it easy to test the full Arduino SPI API:
//   1. Prime transaction  – master sends payload; master receives zeros (ignored)
//   2. Echo  transaction  – master sends zeros; master receives its own payload back
//
// SPDX-License-Identifier: MIT

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Must be >= the largest single SPI transaction the chip will need to handle.
// 512 bytes provides ample headroom for current and future tests.
#define MAX_BUF 512

typedef struct {
  pin_t    cs_pin;
  uint32_t spi;
  uint8_t  spi_buf[1];   // 1-byte window used for continuous byte-by-byte operation
  uint8_t  rx_buf[MAX_BUF]; // bytes received during current CS transaction
  uint8_t  tx_buf[MAX_BUF]; // bytes to send during next CS transaction (echo of previous RX)
  uint32_t rx_idx;           // write cursor for rx_buf during a transaction
  uint32_t tx_idx;           // read cursor for tx_buf during a transaction
} chip_state_t;

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);
static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count);

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  memset(chip->tx_buf, 0, MAX_BUF);
  memset(chip->rx_buf, 0, MAX_BUF);
  chip->rx_idx = 0;
  chip->tx_idx = 0;

  chip->cs_pin = pin_init("CS", INPUT_PULLUP);

  const pin_watch_config_t watch_config = {
    .edge      = BOTH,
    .pin_change = chip_pin_change,
    .user_data  = chip,
  };
  pin_watch(chip->cs_pin, &watch_config);

  const spi_config_t spi_config = {
    .sck      = pin_init("SCK",  INPUT),
    .miso     = pin_init("MISO", INPUT),
    .mosi     = pin_init("MOSI", INPUT),
    .done     = chip_spi_done,
    .user_data = chip,
  };
  chip->spi = spi_init(&spi_config);

  printf("SPI Echo Chip initialized\n");
}

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (pin != chip->cs_pin) {
    return;
  }

  if (value == LOW) {
    // CS asserted: begin a new transaction.
    // tx_buf already holds the echo data from the previous transaction.
    chip->rx_idx = 0;
    chip->tx_idx = 0;
    chip->spi_buf[0] = chip->tx_buf[chip->tx_idx++];
    spi_start(chip->spi, chip->spi_buf, sizeof(chip->spi_buf));
  } else {
    // CS deasserted: snapshot rx_buf → tx_buf for echoing next time.
    uint32_t n = chip->rx_idx;
    memcpy(chip->tx_buf, chip->rx_buf, n);
    // Zero-pad the rest so stale bytes from longer prior transactions are gone.
    if (n < MAX_BUF) {
      memset(chip->tx_buf + n, 0, MAX_BUF - n);
    }
    chip->tx_idx = 0;
    spi_stop(chip->spi);
  }
}

static void chip_spi_done(void *user_data, uint8_t *buffer, uint32_t count) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (!count) {
    // Called from spi_stop with no pending byte – nothing to do.
    return;
  }

  // Store the byte received from the master (MOSI).
  if (chip->rx_idx < MAX_BUF) {
    chip->rx_buf[chip->rx_idx++] = buffer[0];
  }

  // Load the next byte to send to the master (MISO).
  buffer[0] = (chip->tx_idx < MAX_BUF) ? chip->tx_buf[chip->tx_idx++] : 0x00;

  if (pin_read(chip->cs_pin) == LOW) {
    spi_start(chip->spi, chip->spi_buf, sizeof(chip->spi_buf));
  }
}
