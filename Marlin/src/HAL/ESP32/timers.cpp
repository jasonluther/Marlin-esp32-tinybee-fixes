/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#ifdef ARDUINO_ARCH_ESP32

#include <stdio.h>
#include <esp_types.h>
#include <soc/timer_group_struct.h>
//Change: For 5.1.4 #include <driver/periph_ctrl.h>
//#include <driver/timer.h> // Change: For 5.1.4
#include <driver/gptimer.h> // Change: For 5.1.4

#include "../../inc/MarlinConfig.h"

// ------------------------
// Local defines
// ------------------------

#define NUM_HARDWARE_TIMERS 4

// ------------------------
// Private Variables
// ------------------------

static timg_dev_t *TG[2] = {&TIMERG0, &TIMERG1};

const tTimerConfig timer_config[NUM_HARDWARE_TIMERS] = { //Change: For 5.1.4
  { 0, STEPPER_TIMER_PRESCALE, stepTC_Handler, NULL, false }, // 0 - Stepper
  { 1, TEMP_TIMER_PRESCALE, tempTC_Handler, NULL, false },    // 1 - Temperature
  { 2, PWM_TIMER_PRESCALE, pwmTC_Handler, NULL, false },      // 2 - PWM
  { 3, TONE_TIMER_PRESCALE, toneTC_Handler, NULL, false },    // 3 - Tone
};

// ------------------------
// Public functions
// ------------------------
// Change: For 5.1.4
/*
void IRAM_ATTR timer_isr(void *para) {
  const tTimerConfig& timer = timer_config[(int)para];

  // Retrieve the interrupt status and the counter value
  // from the timer that reported the interrupt
  uint32_t intr_status = TG[timer.group]->int_st_timers.val;
  TG[timer.group]->hw_timer[timer.idx].update = 1;

  // Clear the interrupt
  if (intr_status & BIT(timer.idx)) {
    switch (timer.idx) {
      case TIMER_0: TG[timer.group]->int_clr_timers.t0 = 1; break;
      case TIMER_1: TG[timer.group]->int_clr_timers.t1 = 1; break;
      case TIMER_MAX: break;
    }
  }

  timer.fn();

  // After the alarm has been triggered
  // Enable it again so it gets triggered the next time
  TG[timer.group]->hw_timer[timer.idx].config.alarm_en = TIMER_ALARM_EN;
}
*/

bool IRAM_ATTR timer_on_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data) {
  uint8_t timer_num = (uint8_t)(uint32_t)user_data;
  const tTimerConfig& timer_config_entry = timer_config[timer_num];
  // Call the timer callback
  timer_config_entry.fn();
  
  return false; // return false to re-enable the alarm
}


/**
 * Enable and initialize the timer
 * @param timer_num timer number to initialize
 * @param frequency frequency of the timer
 */
void HAL_timer_start(const uint8_t timer_num, const uint32_t frequency) {
  // Change: For 5.1.4
  /*const tTimerConfig timer = timer_config[timer_num];

  timer_config_t config;
  config.divider     = timer.divider;
  config.counter_dir = TIMER_COUNT_UP;
  config.counter_en  = TIMER_PAUSE;
  config.alarm_en    = TIMER_ALARM_EN;
  config.intr_type   = TIMER_INTR_LEVEL;
  config.auto_reload = TIMER_AUTORELOAD_EN; //Change: For 5.1.4

  // Select and initialize the timer
  timer_init(timer.group, timer.idx, &config);

  // Timer counter initial value and auto reload on alarm
  timer_set_counter_value(timer.group, timer.idx, 0x00000000ULL);

  // Configure the alam value and the interrupt on alarm
  timer_set_alarm_value(timer.group, timer.idx, (HAL_TIMER_RATE) / timer.divider / frequency - 1);

  timer_enable_intr(timer.group, timer.idx);

  timer_isr_register(timer.group, timer.idx, timer_isr, (void*)(uint32_t)timer_num, 0, nullptr);

  timer_start(timer.group, timer.idx);
  */
 const tTimerConfig& timer = timer_config[timer_num];
  
  // Timer configuration
  gptimer_config_t config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = HAL_TIMER_RATE / timer.divider,
  };
  
  // Timer initialisation
  ESP_ERROR_CHECK(gptimer_new_timer(&config, &timer_config[timer_num].timer));
  
  // Alarm configuration
  uint64_t alarm_value = (HAL_TIMER_RATE) / timer.divider / frequency - 1;
  gptimer_alarm_config_t alarm_config = {
    .alarm_count = alarm_value,
    .reload_count = 0,
    .flags.auto_reload_on_alarm = true,
  };
  ESP_ERROR_CHECK(gptimer_set_alarm_action(timer_config[timer_num].timer, &alarm_config));
  
  // Callback registration
  gptimer_event_callbacks_t callbacks = {
    .on_alarm = timer_on_alarm_callback,
  };
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer_config[timer_num].timer, &callbacks, (void*)timer_num));
  
  // Start the timer
  ESP_ERROR_CHECK(gptimer_enable(timer_config[timer_num].timer));
  ESP_ERROR_CHECK(gptimer_start(timer_config[timer_num].timer));
}

/**
 * Set the upper value of the timer, when the timer reaches this upper value the
 * interrupt should be triggered and the counter reset
 * @param timer_num timer number to set the compare value to
 * @param compare   threshold at which the interrupt is triggered
 */
// Change: For 5.1.4
/*
 void HAL_timer_set_compare(const uint8_t timer_num, const hal_timer_t compare) {
  const tTimerConfig timer = timer_config[timer_num];
  timer_set_alarm_value(timer.group, timer.idx, compare);
}
*/
void HAL_timer_set_compare(const uint8_t timer_num, const hal_timer_t compare) {
  const tTimerConfig& timer = timer_config[timer_num];
  gptimer_alarm_config_t alarm_config = {
    .alarm_count = compare,
    .reload_count = 0,
    .flags.auto_reload_on_alarm = true,
  };
  ESP_ERROR_CHECK(gptimer_set_alarm_action(timer.timer, &alarm_config));
}

/**
 * Get the current upper value of the timer
 * @param  timer_num timer number to get the count from
 * @return           the timer current threshold for the alarm to be triggered
 */
// Change: For 5.1.4
/*
hal_timer_t HAL_timer_get_compare(const uint8_t timer_num) {
  const tTimerConfig timer = timer_config[timer_num];

  uint64_t alarm_value;
  timer_get_alarm_value(timer.group, timer.idx, &alarm_value);

  return alarm_value;
}
*/
hal_timer_t HAL_timer_get_compare(const uint8_t timer_num) {
  const tTimerConfig& timer = timer_config[timer_num];
  uint64_t alarm_value;
  gptimer_get_raw_count(timer.timer, &alarm_value);
  return alarm_value;
}

/**
 * Get the current counter value between 0 and the maximum count (HAL_timer_set_count)
 * @param  timer_num timer number to get the current count
 * @return           the current counter of the alarm
 */
hal_timer_t HAL_timer_get_count(const uint8_t timer_num) {
  // Change: For 5.1.4
  /*
  const tTimerConfig timer = timer_config[timer_num];
  uint64_t counter_value;
  timer_get_counter_value(timer.group, timer.idx, &counter_value);
  return counter_value;
  */
  const tTimerConfig& timer = timer_config[timer_num];
  uint64_t counter_value;
  ESP_ERROR_CHECK(gptimer_get_raw_count(timer.timer, &counter_value));
  return counter_value;
}

/**
 * Enable timer interrupt on the timer
 * @param timer_num timer number to enable interrupts on
 */
void HAL_timer_enable_interrupt(const uint8_t timer_num) {
  // Change: For 5.1.4
  //const tTimerConfig timer = timer_config[timer_num];
  //timer_enable_intr(timer.group, timer.idx);
  timer_config[timer_num].interrupt_enabled = true;
  if (timer_config[timer_num].timer != NULL) {
    gptimer_event_callbacks_t callbacks = {
      .on_alarm = timer_on_alarm_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer_config[timer_num].timer, &callbacks, (void*)timer_num));
  }
}

/**
 * Disable timer interrupt on the timer
 * @param timer_num timer number to disable interrupts on
 */
void HAL_timer_disable_interrupt(const uint8_t timer_num) {
  // Change: For 5.1.4
  //const tTimerConfig timer = timer_config[timer_num];
  //timer_disable_intr(timer.group, timer.idx);
  timer_config[timer_num].interrupt_enabled = false;
  if (timer_config[timer_num].timer != NULL) {
    gptimer_event_callbacks_t callbacks = {
      .on_alarm = NULL,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer_config[timer_num].timer, &callbacks, NULL));
  }
}

bool HAL_timer_interrupt_enabled(const uint8_t timer_num) {
  // Change: For 5.1.4
  //const tTimerConfig timer = timer_config[timer_num];
  //return TG[timer.group]->int_ena.val | BIT(timer_num);
  return timer_config[timer_num].interrupt_enabled;
}

#endif // ARDUINO_ARCH_ESP32
