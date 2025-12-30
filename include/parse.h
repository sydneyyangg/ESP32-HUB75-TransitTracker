#pragma once

#include "Arduino.h"
// pb
#include "pb_encode.h"
#include "pb_decode.h"
#include "gtfs-realtime.pb.h"
// http client
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
// time
#include "esp_sntp.h"
#include <time.h>

#define MAX_HTTP_RECV_BUFFER 16384
#define ROUTE_ID_MAX 16
#define STOP_ID_MAX 32

extern bool status;
extern int minutes_until;

typedef struct {
    bool route_match;
    bool stop_match;
    bool anyfound;
    int min_minutes;
} ParseState;

// ---- API ----
esp_err_t parse_pb();
void init_time();

// ---- callbacks ----
bool entity_cb(pb_istream_t*, const pb_field_t*, void**);
bool routeid_cb(pb_istream_t*, const pb_field_t*, void**);
bool stoptimeupdates_cb(pb_istream_t*, const pb_field_t*, void**);
bool stopid_cb(pb_istream_t*, const pb_field_t*, void**);

esp_err_t checkErrors(bool ok, pb_istream_t* stream);
