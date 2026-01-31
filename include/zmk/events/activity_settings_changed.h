/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

/**
 * Event raised when activity settings (idle/sleep timeouts) are changed.
 * This event is used to propagate settings changes to split keyboard peripherals.
 */
struct zmk_activity_settings_changed {
    uint32_t idle_ms;
    uint32_t sleep_ms;
    uint8_t source; // 0xFF for self, 0 for central, 1+ for peripherals
};

ZMK_EVENT_DECLARE(zmk_activity_settings_changed);
