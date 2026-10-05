/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <zmk/settings/core.pb.h>

#ifdef NDEBUG
#error "Activity contract tests require assertions enabled"
#endif

/* Only framework/activity/event boundaries are substitutes. The complete
 * production handler, event structures, schema and nanopb implementation are
 * compiled below. These tests do not exercise a Studio or split transport. */
#define LOG_MODULE_DECLARE(...)
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define IS_ENABLED(option) option
#define ZMK_RELAY_EVENT_SOURCE_SELF 255
#define ZMK_EV_EVENT_BUBBLE 0
#define ZMK_EVENT_DECLARE(name)
#define ZMK_EVENT_IMPL(name)
#define ZMK_LISTENER(...)
#define ZMK_SUBSCRIPTION(...)
#define ZMK_RELAY_EVENT_CENTRAL_TO_PERIPHERAL(...)
#define ZMK_RELAY_EVENT_PERIPHERAL_TO_CENTRAL(...)
#define ZMK_RELAY_EVENT_HANDLE(...)
#include <zmk/events/activity_settings_changed.h>
#include <zmk/events/activity_settings_report.h>

typedef struct {
    struct zmk_activity_settings_report *report;
    struct zmk_activity_settings_changed *changed;
    struct zmk_activity_settings_request *request;
} zmk_event_t;
static struct zmk_activity_settings_report *as_zmk_activity_settings_report(const zmk_event_t *e) {
    return e->report;
}
static struct zmk_activity_settings_changed *as_zmk_activity_settings_changed(const zmk_event_t *e) {
    return e->changed;
}
static struct zmk_activity_settings_request *as_zmk_activity_settings_request(const zmk_event_t *e) {
    return e->request;
}
static unsigned writes, changed_events, request_events, report_events;
static uint32_t idle_ms, sleep_ms;
static bool fail_idle, fail_sleep;
static struct zmk_activity_settings_changed changed;
static struct zmk_activity_settings_report reported;
static uint32_t zmk_activity_get_idle_ms(void) { return idle_ms; }
static uint32_t zmk_activity_get_sleep_ms(void) { return sleep_ms; }
static bool zmk_activity_set_idle_ms(uint32_t value) {
    writes++; if (fail_idle) return false; idle_ms = value; return true;
}
static bool zmk_activity_set_sleep_ms(uint32_t value) {
    writes++; if (fail_sleep) return false; sleep_ms = value; return true;
}
static void raise_zmk_activity_settings_changed(struct zmk_activity_settings_changed e) {
    changed = e; changed_events++;
}
static void raise_zmk_activity_settings_request(struct zmk_activity_settings_request e) {
    assert(e.request_id == 0); request_events++;
}
static void raise_zmk_activity_settings_report(struct zmk_activity_settings_report e) {
    reported = e; report_events++;
}

#include CHANGED_SOURCE
#include REPORT_SOURCE

#if TEST_RPC
typedef struct { struct { size_t size; uint8_t bytes[128]; } payload; } zmk_custom_CallRequest;
struct zmk_rpc_custom_subsystem_meta { const char *url; unsigned security; };
#define ZMK_STUDIO_RPC_HANDLER_UNSECURED 0
#define ZMK_RPC_CUSTOM_SUBSYSTEM_UI_URLS(value) .url = value
struct zmk_rpc_custom_subsystem { const char *identifier; };
static struct zmk_rpc_custom_subsystem subsystems[] = {{"other"}, {"zmk__settings"}};
static size_t fake_subsystem_count = 2;
#define STRUCT_SECTION_COUNT(type, count) (*(count) = fake_subsystem_count)
#define STRUCT_SECTION_GET(type, i, value) (*(value) = &subsystems[i])
#define ZMK_RPC_CUSTOM_SUBSYSTEM(id, meta, handler) \
    static bool handler(const zmk_custom_CallRequest *, pb_callback_t *);
static zmk_settings_Response response_buffer;
#define ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(id, type)

static bool encode_response(pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
    size_t size;
    return pb_encode_tag_for_field(stream, field) &&
           pb_get_encoded_size(&size, zmk_settings_Response_fields, *arg) &&
           pb_encode_varint(stream, size) &&
           pb_encode(stream, zmk_settings_Response_fields, *arg);
}
static zmk_settings_Response *allocate_response(pb_callback_t *callback) {
    response_buffer = (zmk_settings_Response)zmk_settings_Response_init_zero;
    callback->funcs.encode = encode_response;
    callback->arg = &response_buffer;
    return &response_buffer;
}
#define ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(id, callback) allocate_response(callback)
struct zmk_studio_custom_notification { uint8_t subsystem_index; pb_callback_t encode_payload; };
static zmk_settings_Notification notifications[4];
static unsigned notification_count;

/* Both response and notification payloads are encoded through the callbacks
 * supplied to the framework. Invoke twice, as nanopb does for sizing/output. */
static size_t encode_payload(pb_callback_t callback, uint8_t *wire, size_t capacity) {
    assert(callback.funcs.encode && callback.arg);
    pb_field_t field = {0}; field.tag = 1; field.type = PB_LTYPE_BYTES;
    pb_ostream_t sizing = PB_OSTREAM_SIZING;
    assert(callback.funcs.encode(&sizing, &field, &callback.arg));
    pb_ostream_t output = pb_ostream_from_buffer(wire, capacity);
    assert(callback.funcs.encode(&output, &field, &callback.arg));
    assert(output.bytes_written == sizing.bytes_written);
    return output.bytes_written;
}
static void decode_payload(const uint8_t *wire, size_t size, const pb_msgdesc_t *fields, void *value) {
    pb_istream_t input = pb_istream_from_buffer(wire, size);
    pb_wire_type_t type; uint32_t tag; bool eof;
    assert(pb_decode_tag(&input, &type, &tag, &eof) && !eof && type == PB_WT_STRING && tag == 1);
    pb_istream_t payload;
    assert(pb_make_string_substream(&input, &payload));
    assert(pb_decode(&payload, fields, value));
    assert(pb_close_string_substream(&input, &payload) && input.bytes_left == 0);
}
static void raise_zmk_studio_custom_notification(struct zmk_studio_custom_notification event) {
    assert(event.subsystem_index == 1 && notification_count < 4);
    uint8_t wire[128];
    size_t size = encode_payload(event.encode_payload, wire, sizeof(wire));
    zmk_settings_Notification *n = &notifications[notification_count++];
    *n = (zmk_settings_Notification)zmk_settings_Notification_init_zero;
    decode_payload(wire, size, zmk_settings_Notification_fields, n);
    assert(n->which_notification_type == zmk_settings_Notification_activity_settings_tag);
}

#include RPC_SOURCE
#endif

static void reset(void) {
    idle_ms = 1200; sleep_ms = 3400;
    writes = changed_events = request_events = report_events = 0;
    fail_idle = fail_sleep = false;
#if TEST_RPC
    notification_count = 0; fake_subsystem_count = 2;
#endif
}
static const uint32_t values[][2] = {{1200, 3400}, {0, 0}, {0, 17}, {19, 0}, {UINT32_MAX, UINT32_MAX}};

static void test_apply_event(void) {
    reset(); zmk_event_t empty = {0};
    assert(activity_settings_changed_listener(&empty) == ZMK_EV_EVENT_BUBBLE && writes == 0);
    for (unsigned i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
        struct zmk_activity_settings_changed e = {.idle_ms=values[i][0], .sleep_ms=values[i][1], .source=255};
        zmk_event_t event = {.changed=&e};
        unsigned before_writes = writes;
        uint32_t before_idle = idle_ms, before_sleep = sleep_ms;
        assert(activity_settings_changed_listener(&event) == ZMK_EV_EVENT_BUBBLE);
        assert(writes == before_writes && idle_ms == before_idle && sleep_ms == before_sleep);
        e.source = i;
        assert(activity_settings_changed_listener(&event) == ZMK_EV_EVENT_BUBBLE);
        assert(writes == before_writes + 2 && idle_ms == values[i][0] && sleep_ms == values[i][1]);
    }
    assert(changed_events == 0 && request_events == 0 && report_events == 0);
}
#if CONFIG_ZMK_SPLIT && !CONFIG_ZMK_SPLIT_ROLE_CENTRAL
static void test_request_event(void) {
    reset(); zmk_event_t empty = {0};
    assert(activity_settings_request_listener(&empty) == ZMK_EV_EVENT_BUBBLE && report_events == 0);
    for (unsigned i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
        idle_ms = values[i][0]; sleep_ms = values[i][1];
        struct zmk_activity_settings_request req = {.request_id=i == 0 ? 0 : 255};
        zmk_event_t event = {.request=&req};
        assert(activity_settings_request_listener(&event) == ZMK_EV_EVENT_BUBBLE);
        assert(report_events == i + 1 && reported.request_id == req.request_id);
        assert(reported.idle_ms == idle_ms && reported.sleep_ms == sleep_ms && reported.source == 255);
    }
    assert(writes == 0 && changed_events == 0 && request_events == 0);
}
#endif
#if TEST_RPC
static zmk_settings_Response call_raw(const uint8_t *bytes, size_t size) {
    zmk_custom_CallRequest raw = {0};
    assert(size <= sizeof(raw.payload.bytes));
    memcpy(raw.payload.bytes, bytes, size); raw.payload.size = size;
    pb_callback_t callback = {0};
    assert(settings_rpc_handle_request(&raw, &callback));
    unsigned before_writes = writes, before_events = changed_events + request_events + notification_count;
    uint8_t wire[128];
    size = encode_payload(callback, wire, sizeof(wire));
    assert(writes == before_writes && changed_events + request_events + notification_count == before_events);
    zmk_settings_Response response = zmk_settings_Response_init_zero;
    decode_payload(wire, size, zmk_settings_Response_fields, &response);
    return response;
}
static zmk_settings_Response call(zmk_settings_Request request) {
    uint8_t wire[128]; pb_ostream_t output = pb_ostream_from_buffer(wire, sizeof(wire));
    assert(pb_encode(&output, zmk_settings_Request_fields, &request));
    return call_raw(wire, output.bytes_written);
}
static zmk_settings_Request request(pb_size_t tag) {
    zmk_settings_Request req = zmk_settings_Request_init_zero;
    req.which_request_type = tag;
    return req;
}
static void test_get(void) {
    reset();
    for (unsigned i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
        idle_ms = values[i][0]; sleep_ms = values[i][1];
        zmk_settings_Response resp = call(request(zmk_settings_Request_get_activity_settings_tag));
        assert(resp.which_response_type == zmk_settings_Response_get_activity_settings_tag);
        assert(resp.response_type.get_activity_settings.has_settings);
        zmk_settings_ActivitySettings s = resp.response_type.get_activity_settings.settings;
        assert(s.idle_ms == idle_ms && s.sleep_ms == sleep_ms && s.source == 0);
        assert(writes == 0 && changed_events == 0 && request_events == 0 && notification_count == 0);
    }
}
static void assert_notification(unsigned index, uint32_t idle, uint32_t sleep, uint32_t source) {
    zmk_settings_ActivitySettingsNotification n = notifications[index].notification_type.activity_settings;
    assert(n.has_settings && n.settings.idle_ms == idle && n.settings.sleep_ms == sleep && n.settings.source == source);
}
static void test_get_all(void) {
    for (unsigned i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
        reset(); idle_ms = values[i][0]; sleep_ms = values[i][1];
        zmk_settings_Response resp = call(request(zmk_settings_Request_get_all_activity_settings_tag));
        assert(resp.which_response_type == zmk_settings_Response_get_all_activity_settings_tag);
        assert(resp.response_type.get_all_activity_settings.request_sent);
        assert(notification_count == 1 && writes == 0 && changed_events == 0);
        assert_notification(0, idle_ms, sleep_ms, 0);
        assert(request_events == CONFIG_ZMK_SPLIT_ROLE_CENTRAL);
    }
    /* A missing registry entry suppresses notifications; a subsequent request
     * can recover. GetAll's existing fire-and-forget response is unchanged. */
    reset(); fake_subsystem_count = 1;
    (void)call(request(zmk_settings_Request_get_all_activity_settings_tag));
    assert(notification_count == 0);
    fake_subsystem_count = 2;
    (void)call(request(zmk_settings_Request_get_all_activity_settings_tag));
    assert(notification_count == 1);
    assert_notification(0, 1200, 3400, 0);
}
static void test_set(void) {
    for (unsigned i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
        reset();
        zmk_settings_Request req = request(zmk_settings_Request_set_activity_settings_tag);
        req.request_type.set_activity_settings.has_settings = true;
        req.request_type.set_activity_settings.settings.idle_ms = values[i][0];
        req.request_type.set_activity_settings.settings.sleep_ms = values[i][1];
        zmk_settings_Response resp = call(req);
        assert(resp.which_response_type == zmk_settings_Response_set_activity_settings_tag);
        assert(resp.response_type.set_activity_settings.success && writes == 2);
        assert(idle_ms == values[i][0] && sleep_ms == values[i][1]);
        assert(changed_events == CONFIG_ZMK_SPLIT_RELAY_EVENT && notification_count == 0 && request_events == 0);
        if (changed_events) {
            assert(changed.idle_ms == idle_ms && changed.sleep_ms == sleep_ms && changed.source == 255);
        }
        /* Getter must observe the just-applied values through a new callback. */
        resp = call(request(zmk_settings_Request_get_activity_settings_tag));
        assert(resp.which_response_type == zmk_settings_Response_get_activity_settings_tag);
        assert(resp.response_type.get_activity_settings.has_settings);
        assert(resp.response_type.get_activity_settings.settings.idle_ms == idle_ms);
        assert(resp.response_type.get_activity_settings.settings.sleep_ms == sleep_ms && writes == 2);
    }
    for (unsigned i = 0; i < 2; i++) {
        reset(); fail_idle = i == 0; fail_sleep = i == 1;
        zmk_settings_Request req = request(zmk_settings_Request_set_activity_settings_tag);
        req.request_type.set_activity_settings.has_settings = true;
        req.request_type.set_activity_settings.settings.idle_ms = 1000;
        req.request_type.set_activity_settings.settings.sleep_ms = 2000;
        zmk_settings_Response resp = call(req);
        assert(resp.which_response_type == zmk_settings_Response_error_tag);
        assert(writes == 2 && changed_events == 0 && notification_count == 0 && request_events == 0);
        fail_idle = fail_sleep = false;
        resp = call(req);
        assert(resp.which_response_type == zmk_settings_Response_set_activity_settings_tag);
        assert(resp.response_type.set_activity_settings.success && idle_ms == 1000 && sleep_ms == 2000);
    }
}
static void test_errors(void) {
    reset();
    const uint8_t malformed[] = {0x12, 0x04, 0x0a};
    zmk_settings_Response resp = call_raw(malformed, sizeof(malformed));
    assert(resp.which_response_type == zmk_settings_Response_error_tag);
    assert(strcmp(resp.response_type.error.message, "Failed to decode request") == 0);
    resp = call(request(0));
    assert(resp.which_response_type == zmk_settings_Response_error_tag);
    assert(strcmp(resp.response_type.error.message, "Failed to process request") == 0);
    assert(writes == 0 && changed_events == 0 && request_events == 0 && notification_count == 0);
    /* A decode/dispatch error must not leave stale response data. */
    resp = call(request(zmk_settings_Request_get_activity_settings_tag));
    assert(resp.which_response_type == zmk_settings_Response_get_activity_settings_tag);
    assert(resp.response_type.get_activity_settings.has_settings);
}
#if CONFIG_ZMK_SPLIT_RELAY_EVENT
static void test_report(void) {
    reset(); zmk_event_t empty = {0};
    assert(activity_settings_report_listener(&empty) == ZMK_EV_EVENT_BUBBLE && notification_count == 0);
    for (unsigned i = 0; i < 3; i++) {
        struct zmk_activity_settings_report report = {.idle_ms=values[i][0], .sleep_ms=values[i][1], .source=i+1};
        zmk_event_t event = {.report=&report};
        assert(activity_settings_report_listener(&event) == ZMK_EV_EVENT_BUBBLE);
        assert(notification_count == i + 1);
        assert_notification(i, report.idle_ms, report.sleep_ms, report.source);
    }
    assert(writes == 0 && changed_events == 0 && request_events == 0);
}
#endif
#endif /* TEST_RPC */
int main(void) {
    test_apply_event(); unsigned groups = 1;
#if TEST_RPC
    test_get(); test_get_all(); test_set(); test_errors();
    groups += 4;
#if CONFIG_ZMK_SPLIT_RELAY_EVENT
    test_report(); groups++;
#endif
    (void)settings_rpc_meta;
#endif
#if CONFIG_ZMK_SPLIT && !CONFIG_ZMK_SPLIT_ROLE_CENTRAL
    test_request_event(); groups++;
#endif
    printf("activity settings: PASS variant=%s role=%s groups=%u\n", TEST_VARIANT, TEST_ROLE, groups);
    return 0;
}
