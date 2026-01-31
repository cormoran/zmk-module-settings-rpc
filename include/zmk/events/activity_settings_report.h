/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

/**
 * Event raised to request activity settings from peripherals.
 * Sent from central to peripherals to collect their current settings.
 */
struct zmk_activity_settings_request {
    uint8_t request_id; // Unique ID to correlate requests and responses
};

ZMK_EVENT_DECLARE(zmk_activity_settings_request);

/**
 * Event raised to report activity settings from a peripheral.
 * Sent from peripheral to central in response to a settings request.
 */
struct zmk_activity_settings_report {
    uint32_t idle_ms;
    uint32_t sleep_ms;
    uint8_t source;      // Source device (0 = central, 1+ = peripheral index)
    uint8_t request_id;  // Matches the request_id from the request
};

ZMK_EVENT_DECLARE(zmk_activity_settings_report);
