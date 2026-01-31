/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/logging/log.h>
#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_settings_report.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

ZMK_EVENT_IMPL(zmk_activity_settings_request);
ZMK_EVENT_IMPL(zmk_activity_settings_report);

#if IS_ENABLED(CONFIG_ZMK_SPLIT)

// Event relay: settings report from peripheral to central
ZMK_RELAY_EVENT_PERIPHERAL_TO_CENTRAL(zmk_activity_settings_report, srp,
                                      source);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

// Handle settings request events (called on peripherals)
ZMK_RELAY_EVENT_HANDLE(zmk_activity_settings_request, srq, );

/**
 * Event listener to respond to settings requests (on peripherals)
 */
static int activity_settings_request_listener(const zmk_event_t *eh) {
    struct zmk_activity_settings_request *ev =
        as_zmk_activity_settings_request(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    // Get current settings and report back
    struct zmk_activity_settings_report report = {
        .idle_ms  = zmk_activity_get_idle_ms(),
        .sleep_ms = zmk_activity_get_sleep_ms(),
        .source = ZMK_RELAY_EVENT_SOURCE_SELF,  // Will be updated by relay with
                                                // actual source
        .request_id = ev->request_id,
    };

    raise_zmk_activity_settings_report(report);
    LOG_DBG("Reported settings: idle=%d, sleep=%d for request %d",
            report.idle_ms, report.sleep_ms, ev->request_id);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(activity_settings_request_handler,
             activity_settings_request_listener);
ZMK_SUBSCRIPTION(activity_settings_request_handler,
                 zmk_activity_settings_request);
#endif  // !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#endif  // IS_ENABLED(CONFIG_ZMK_SPLIT)