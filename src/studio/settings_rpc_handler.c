/**
 * ZMK Core Settings - Custom Studio RPC Handler
 *
 * This file implements custom RPC handlers for getting and setting
 * ZMK core settings like sleep/idle timeouts, with support for
 * split keyboard peripheral synchronization.
 */

#include <pb_decode.h>
#include <pb_encode.h>
#include <zmk/studio/custom.h>
#include <zmk/settings/core.pb.h>
#include <zmk/activity.h>
#include <zmk/events/activity_settings_changed.h>
#include <zmk/event_manager.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/**
 * Metadata for the custom subsystem.
 * - ui_urls: URLs where the custom UI can be loaded from
 * - security: Security level for the RPC handler
 */
static struct zmk_rpc_custom_subsystem_meta settings_rpc_meta = {
    ZMK_RPC_CUSTOM_SUBSYSTEM_UI_URLS("http://localhost:5173"),
    // Unsecured is suggested by default to avoid unlocking in unreliable environments
    .security = ZMK_STUDIO_RPC_HANDLER_UNSECURED,
};

/**
 * Register the custom RPC subsystem.
 * The first argument is the subsystem name used to route requests from the frontend.
 * Format: <namespace>__<feature> (double underscore)
 */
ZMK_RPC_CUSTOM_SUBSYSTEM(zmk__settings, &settings_rpc_meta, settings_rpc_handle_request);

ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(zmk__settings, zmk_settings_Response);

static int handle_get_activity_settings(const zmk_settings_GetActivitySettingsRequest *req,
                                        zmk_settings_Response *resp);
static int handle_set_activity_settings(const zmk_settings_SetActivitySettingsRequest *req,
                                        zmk_settings_Response *resp);

/**
 * Main request handler for the settings RPC subsystem.
 * Sets up the encoding callback for the response.
 */
static bool settings_rpc_handle_request(const zmk_custom_CallRequest *raw_request,
                                        pb_callback_t *encode_response) {
    zmk_settings_Response *resp =
        ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(zmk__settings, encode_response);

    zmk_settings_Request req = zmk_settings_Request_init_zero;

    // Decode the incoming request from the raw payload
    pb_istream_t req_stream =
        pb_istream_from_buffer(raw_request->payload.bytes, raw_request->payload.size);
    if (!pb_decode(&req_stream, zmk_settings_Request_fields, &req)) {
        LOG_WRN("Failed to decode settings request: %s", PB_GET_ERROR(&req_stream));
        zmk_settings_ErrorResponse err = zmk_settings_ErrorResponse_init_zero;
        snprintf(err.message, sizeof(err.message), "Failed to decode request");
        resp->which_response_type = zmk_settings_Response_error_tag;
        resp->response_type.error = err;
        return true;
    }

    int rc = 0;
    switch (req.which_request_type) {
    case zmk_settings_Request_get_activity_settings_tag:
        rc = handle_get_activity_settings(&req.request_type.get_activity_settings, resp);
        break;
    case zmk_settings_Request_set_activity_settings_tag:
        rc = handle_set_activity_settings(&req.request_type.set_activity_settings, resp);
        break;
    default:
        LOG_WRN("Unsupported settings request type: %d", req.which_request_type);
        rc = -1;
    }

    if (rc != 0) {
        zmk_settings_ErrorResponse err = zmk_settings_ErrorResponse_init_zero;
        snprintf(err.message, sizeof(err.message), "Failed to process request");
        resp->which_response_type = zmk_settings_Response_error_tag;
        resp->response_type.error = err;
    }
    return true;
}

/**
 * Handle GetActivitySettings request - returns current sleep/idle timeouts
 */
static int handle_get_activity_settings(const zmk_settings_GetActivitySettingsRequest *req,
                                        zmk_settings_Response *resp) {
    LOG_DBG("Received get activity settings request");

    zmk_settings_GetActivitySettingsResponse result =
        zmk_settings_GetActivitySettingsResponse_init_zero;

    // Get current settings from ZMK activity subsystem
    result.settings.idle_ms = zmk_activity_get_idle_ms();
    result.settings.sleep_ms = zmk_activity_get_sleep_ms();

    LOG_DBG("Current activity settings: idle=%d ms, sleep=%d ms", result.settings.idle_ms,
            result.settings.sleep_ms);

    resp->which_response_type = zmk_settings_Response_get_activity_settings_tag;
    resp->response_type.get_activity_settings = result;
    return 0;
}

/**
 * Handle SetActivitySettings request - updates sleep/idle timeouts
 * and propagates to peripherals via events
 */
static int handle_set_activity_settings(const zmk_settings_SetActivitySettingsRequest *req,
                                        zmk_settings_Response *resp) {
    LOG_DBG("Received set activity settings request: idle=%d ms, sleep=%d ms",
            req->settings.idle_ms, req->settings.sleep_ms);

    bool success = true;

    // Update idle timeout
    if (!zmk_activity_set_idle_ms(req->settings.idle_ms)) {
        LOG_ERR("Failed to set idle timeout to %d ms", req->settings.idle_ms);
        success = false;
    }

    // Update sleep timeout
    if (!zmk_activity_set_sleep_ms(req->settings.sleep_ms)) {
        LOG_ERR("Failed to set sleep timeout to %d ms", req->settings.sleep_ms);
        success = false;
    }

    if (success) {
        // Raise event to propagate to peripherals
        struct zmk_activity_settings_changed event = {
            .idle_ms = req->settings.idle_ms,
            .sleep_ms = req->settings.sleep_ms,
            .source = ZMK_RELAY_EVENT_SOURCE_SELF,
        };
        raise_zmk_activity_settings_changed(event);
        LOG_DBG("Activity settings updated and event raised");
    }

    zmk_settings_SetActivitySettingsResponse result =
        zmk_settings_SetActivitySettingsResponse_init_zero;
    result.success = success;

    resp->which_response_type = zmk_settings_Response_set_activity_settings_tag;
    resp->response_type.set_activity_settings = result;
    return success ? 0 : -1;
}

#if IS_ENABLED(CONFIG_ZMK_SPLIT_RELAY_EVENT)

// Event relay: central to peripheral
ZMK_RELAY_EVENT_CENTRAL_TO_PERIPHERAL(zmk_activity_settings_changed, activity_settings, source);

// Event relay: peripheral to central
ZMK_RELAY_EVENT_PERIPHERAL_TO_CENTRAL(zmk_activity_settings_changed, activity_settings, source);

// Handle relayed events to apply settings
ZMK_RELAY_EVENT_HANDLE(zmk_activity_settings_changed, activity_settings, source);

/**
 * Event listener to apply activity settings when relay event is received
 */
static int activity_settings_changed_listener(const zmk_event_t *eh) {
    struct zmk_activity_settings_changed *ev = as_zmk_activity_settings_changed(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    // Only apply settings from relayed events (not self-originated)
    if (ev->source != ZMK_RELAY_EVENT_SOURCE_SELF) {
        LOG_DBG("Applying relayed activity settings: idle=%d ms, sleep=%d ms from source %d",
                ev->idle_ms, ev->sleep_ms, ev->source);

        zmk_activity_set_idle_ms(ev->idle_ms);
        zmk_activity_set_sleep_ms(ev->sleep_ms);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(activity_settings_apply, activity_settings_changed_listener);
ZMK_SUBSCRIPTION(activity_settings_apply, zmk_activity_settings_changed);

#endif // IS_ENABLED(CONFIG_ZMK_SPLIT_RELAY_EVENT)
