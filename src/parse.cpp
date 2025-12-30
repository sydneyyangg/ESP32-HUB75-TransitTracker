#include "parse.h"

// Global definitions (declared extern in parse.h)

bool status = false;
int minutes_until = 0;

//fn defs
bool entity_cb(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool routeid_cb(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool stoptimeupdates_cb(pb_istream_t *stream, const pb_field_t *field, void **arg);
void init_time(void);
bool stopid_cb(pb_istream_t *stream, const pb_field_t *field, void **arg);

esp_err_t checkErrors(bool status, pb_istream_t *stream);
esp_err_t http_fetch_pb(uint8_t *out_buf, int max_len, int *out_len);
bool http_pb_read(pb_istream_t *stream, uint8_t *buf, size_t count);

struct HttpPbStreamCtx {
    esp_http_client_handle_t client;
};

esp_err_t parse_pb(){    
    // config http connection
    esp_http_client_config_t config = {
        .url = "https://webapps.regionofwaterloo.ca/api/grt-routes/api/tripupdates/1",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .buffer_size = 8192,      // Larger buffer (default is 512)
        .buffer_size_tx = 2048,   // Larger TX buffer
        .crt_bundle_attach = esp_crt_bundle_attach,

    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;
    if (esp_http_client_set_header(client, "Accept-Encoding", "identity") == ESP_FAIL)
         Serial.println("Could not set header to 'Accept-Encoding', 'identity'");

    if (esp_http_client_set_header(client, "Connection", "close") == ESP_FAIL)
         Serial.println("Could not set header to 'Connection', 'close'");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    Serial.printf("HTTP Status: %d\n", status_code);
    Serial.printf("Content-Length: %d\n", content_length);

    if (status_code != 200) {
        Serial.printf("HTTP error: %d\n", status_code);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    if (content_length == 0) {
        if(esp_http_client_is_chunked_response(client)){Serial.println("The content is chunked.");}
        else {Serial.println("The content is NOT chunked.");}
        
        // Content-Length not provided, try reading anyway
        Serial.println("Using chunked transfer or unknown length");
        content_length = INT_MAX; // or some reasonable max
    }

    else if (content_length == -1) {
        Serial.println("esp_http_client_fetch_headers returned ESP_FAIL");
        return ESP_FAIL;
    }

    else if (content_length == -0x7007){
        Serial.println("esp_http_client_fetch_headers returned ESP_ERR_HTTP_EAGAIN");
        return ESP_ERR_HTTP_EAGAIN;
    }
    
    HttpPbStreamCtx ctx = {
    .client = client
    };

    pb_istream_t stream = {
    .callback = http_pb_read,
    .state = &ctx,
    .bytes_left = SIZE_MAX  // Let nanopb manage this as bytes_left is changed in sub-streams
    };

    // first, configure feedMsg
     // feedmsg.entity is a callback hook, not a data retrieval
    // decode is a function executed when entity is found
    // arg is an argument for that function 
    transit_realtime_FeedMessage feedmsg = transit_realtime_FeedMessage_init_zero;
    ParseState state = {0};
    feedmsg.entity.arg = &state;  
    feedmsg.entity.funcs.decode = entity_cb;
    bool decode_ok = pb_decode(&stream, &transit_realtime_FeedMessage_msg, &feedmsg);
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (state.anyfound){
        minutes_until = state.min_minutes;
        Serial.printf("*** Soonest! Route 201, Stop 4072, %d minutes ***\n", minutes_until);
        return ESP_OK;
    }

    if (!decode_ok && !state.anyfound) {
        Serial.printf("Decoding failed or didn't find a state: %s\n", PB_GET_ERROR(&stream));
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool http_pb_read(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    HttpPbStreamCtx *ctx = (HttpPbStreamCtx *)stream->state;
    size_t total = 0;

    // for streaming pb as an http client
    while (total < count) {
        int r = esp_http_client_read(
            ctx->client,
            (char *)(buf + total),
            count - total
        );

        if (r < 0) {
            Serial.printf("  Read error: %d\n", r);
            return false;
        }
        
        if (r == 0) {
            Serial.printf("  End of stream (read 0 bytes), total read: %d of %d\n", total, count);
            return false;
        }

        total += r;
    }    
    return true;
}


bool entity_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){

    ParseState *parseCheck = (ParseState *)(*arg);

    transit_realtime_FeedEntity currententity = transit_realtime_FeedEntity_init_zero;

    // Reset flags for this entity
    parseCheck->route_match = false;
    parseCheck->stop_match = false;

    // entity => tripupdate => route_id must be ... (201) or ... [later]
    currententity.trip_update.trip.route_id.funcs.decode = routeid_cb;
    currententity.trip_update.trip.route_id.arg = parseCheck;

    currententity.trip_update.stop_time_update.funcs.decode = stoptimeupdates_cb;
    currententity.trip_update.stop_time_update.arg = parseCheck;

    parseCheck->route_match = false;
    parseCheck->stop_match = false;

    status = pb_decode(stream, &transit_realtime_FeedEntity_msg, &currententity);
    
    if (!status) {
        checkErrors(status, stream);
        return false;
    }

    // Entity must have trip update to proceed
    if (!currententity.has_trip_update) {
        return true;
    }

    return true;
}

//pb_callback_t _transit_realtime_TripDescriptor::route_id
//The route_id from the GTFS that this selector refers to.
bool routeid_cb(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    char route_id[ROUTE_ID_MAX];

    size_t len = stream->bytes_left;
    if (len >= sizeof(route_id)) len = sizeof(route_id) - 1;

    if (!pb_read(stream, (uint8_t *)route_id, len)) return false;
    route_id[len] = '\0';

    Serial.printf("Route ID seen: %s\n", route_id);

    ParseState *st = (ParseState *)(*arg);
    st->route_match = (strcmp(route_id, "201") == 0);

    return true;
}


bool stoptimeupdates_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
    ParseState *parseCheck = (ParseState *)(*arg);

     // Check if we're on the right route FIRST
    if (!parseCheck->route_match) {
        // Wrong route, skip this stop_time_update without processing
        pb_skip_field(stream, PB_WT_STRING);  // Skip this field
        return true;
    }

    transit_realtime_TripUpdate_StopTimeUpdate stoptimeupdatemsg = transit_realtime_TripUpdate_StopTimeUpdate_init_zero;
    stoptimeupdatemsg.stop_id.funcs.decode = stopid_cb;
    stoptimeupdatemsg.stop_id.arg = parseCheck;

    parseCheck->stop_match = false;
    status = pb_decode(stream, &transit_realtime_TripUpdate_StopTimeUpdate_msg, &stoptimeupdatemsg);
    checkErrors(status, stream);
    if (!status) return false;

    // quit if does not have arrival update
    if (!stoptimeupdatemsg.has_arrival) {return true;}
    // quit if stop_id is not desired stop
    if (!parseCheck->stop_match){return true;}

    // Calculate arrival time
    int64_t arrival = stoptimeupdatemsg.arrival.has_time 
        ? stoptimeupdatemsg.arrival.time 
        : stoptimeupdatemsg.arrival.scheduled_time;
    
    if (stoptimeupdatemsg.arrival.has_delay) {
        arrival += stoptimeupdatemsg.arrival.delay; // arrivetime holds the number of seconds since posix time of the arrival
    }

    time_t now;
    time(&now);
    int current_sec = arrival - now;
    int current_min = 0;
    if (current_sec < 0) {
        current_min = 123;   // already arrived
    } else {
        current_min = current_sec / 60;
    }
    
    if (!parseCheck->anyfound || (current_min < parseCheck->min_minutes)){
        parseCheck->min_minutes = current_min;
        parseCheck->anyfound = true;
    }

    Serial.printf("Found candidate: Route 201, Stop 4072, %d minutes\n", current_min);
    return true;   
}

bool stopid_cb(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    char stop_id[STOP_ID_MAX];

    size_t len = stream->bytes_left;
    if (len >= sizeof(stop_id)) len = sizeof(stop_id) - 1;

    if (!pb_read(stream, (uint8_t *)stop_id, len)) return false;
    stop_id[len] = '\0';

    Serial.printf("Stop ID: %s\n", stop_id);

    ParseState *st = (ParseState *)(*arg);
    st->stop_match = (strcmp(stop_id, "4072") == 0);

    return true;
}


void init_time(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    // Wait for SNTP to sync time, with short timeout
    int attempts = 0;
    const int max_attempts = 20; // ~10 seconds total with 500ms delay
    while (attempts < max_attempts) {
        sntp_sync_status_t status = sntp_get_sync_status();
        if (status == SNTP_SYNC_STATUS_COMPLETED) {
            break;
        }
        // brief wait before re-checking
        vTaskDelay(500 / portTICK_PERIOD_MS);
        attempts++;
    }

    // Optional: small extra wait to let system time propagate
    vTaskDelay(200 / portTICK_PERIOD_MS);
}

esp_err_t checkErrors(bool status, pb_istream_t *stream){
    /* Check for errors... */
    if (!status)
    {
        Serial.printf("CheckErrors(): Decoding failed: %s\n", PB_GET_ERROR(stream));
        return ESP_FAIL;
    }
    return ESP_OK;
}