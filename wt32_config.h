#pragma once

#include <Arduino.h>

static const gpio_num_t PIN_OPTO_POWER = GPIO_NUM_13;
static const gpio_num_t PIN_OPTO_RESET = GPIO_NUM_14;
static const gpio_num_t PIN_LED_STATUS = GPIO_NUM_4;
static const gpio_num_t PIN_BTN_FACTORY = GPIO_NUM_35;

static const uint32_t SERIAL_BAUD = 115200;
static const uint32_t PULSE_SHORT_MS = 300;
static const uint32_t PULSE_LONG_MS = 3000;
static const uint32_t FACTORY_HOLD_MS = 5000;

static const char *NVS_NAMESPACE = "pceth";
static const char *NVS_KEY_WEB_PASS = "web_pass";
static const char *DEFAULT_WEB_PASS = "12345678";

static const size_t WEB_PASS_MAX_LEN = 64;
static const size_t NVS_STR_SHORT = 64;
static const size_t NVS_STR_MED = 96;
static const size_t NVS_STR_LONG = 128;
static const size_t NVS_STR_URL = 200;

static const char *EMPTY_LABEL = "()";

static const uint32_t DEFAULT_CPU_MHZ = 80;
static const uint32_t DEFAULT_LOOP_DELAY_MS = 10;
static const uint32_t LOOP_DELAY_MS_MAX = 100;
