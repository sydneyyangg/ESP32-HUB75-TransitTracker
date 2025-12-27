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



esp_err_t parse_pb(){
    if (!pb_buf) {
        Serial.println("error allocating pb_buf");
        return ESP_FAIL;
    };
    Serial.println("parse_pb() called");
    esp_err_t fetch_status = http_fetch_pb(pb_buf, MAX_HTTP_RECV_BUFFER, &pb_len);
    if (fetch_status != ESP_OK) {
        Serial.println("Parse_pb(): Failed to fetch protobuf feed");
        return fetch_status;
    }
    Serial.printf("Fetched %d bytes\n", pb_len);

    // make the buffer a pb stream
    pb_istream_t stream = pb_istream_from_buffer(pb_buf, size_t(pb_len));

    // first, configure feedMsg
     // feedmsg.entity is a callback hook, not a data retrieval
    // decode is a function executed when entity is found
    // arg is an argument for that function 
    transit_realtime_FeedMessage feedmsg = transit_realtime_FeedMessage_init_zero;
    ParseState state = {0};
    feedmsg.entity.arg = &state;  
    feedmsg.entity.funcs.decode = entity_cb;
    bool decode_ok = pb_decode(&stream, &transit_realtime_FeedMessage_msg, &feedmsg);

    // Allow early abort from callbacks when target is found
    if (!decode_ok) {
        checkErrors(decode_ok, &stream);
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool entity_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
    Serial.println("entity_cb()");

    transit_realtime_FeedEntity currententity = transit_realtime_FeedEntity_init_zero;
    ParseState *parseCheck = (ParseState *)(*arg);

    // entity => tripupdate => route_id must be ... (201) or ... [later]
    currententity.trip_update.trip.route_id.funcs.decode = routeid_cb;
    currententity.trip_update.trip.route_id.arg = parseCheck;

    currententity.trip_update.stop_time_update.funcs.decode = stoptimeupdates_cb;
    currententity.trip_update.stop_time_update.arg = parseCheck;

    parseCheck->route_match = false;
    parseCheck->stop_match = false;

    status = pb_decode(stream, &transit_realtime_FeedEntity_msg, &currententity);
    // If we aborted decode because we found the target, don't treat as error
    if (!status && parseCheck->found) {
        return false;
    }
    checkErrors(status, stream);
    if (!status) return false;

    // entity must have trip update to proceed
    if (!currententity.has_trip_update){return true;}
    // route_id check
    if (parseCheck->route_match == false){return true;}

    return true;
}

//pb_callback_t _transit_realtime_TripDescriptor::route_id
//The route_id from the GTFS that this selector refers to.
bool routeid_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
    Serial.println("routeid_cb()");

    char route_id[ROUTE_ID_MAX];   // adjust size if needed

    size_t len = stream->bytes_left;
    if (len >= sizeof(route_id)) len = sizeof(route_id) - 1;

    if (!pb_read(stream, (uint8_t *)route_id, len)) return false;
    route_id[len] = '\0';

    Serial.print("Route ID seen: ");
    Serial.println(route_id);

    ParseState *st = (ParseState *)(*arg);

    if (strcmp(route_id, "201") != 0) { 
        st->route_match = false; // route not matched
        return true;   
    }

    st->route_match = true;     // mark route matched
    
    return true;
}

bool stoptimeupdates_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
    Serial.println("stoptimeupdates_cb()");
    transit_realtime_TripUpdate_StopTimeUpdate stoptimeupdatemsg = transit_realtime_TripUpdate_StopTimeUpdate_init_zero;
    ParseState *parseCheck = (ParseState *)(*arg);

    stoptimeupdatemsg.stop_id.funcs.decode = stopid_cb;
    stoptimeupdatemsg.stop_id.arg = parseCheck;

    parseCheck->stop_match = false;
    status = pb_decode(stream, &transit_realtime_TripUpdate_StopTimeUpdate_msg, &stoptimeupdatemsg);
    checkErrors(status, stream);
    if (!status) return false;

    // quit if does not have arrival update
    if (!stoptimeupdatemsg.has_arrival) {return true;}
    // quit if stop_id is not desired stop
    if (parseCheck->stop_match == false){return true;}

    // use given time if provided
    if (stoptimeupdatemsg.arrival.has_time){
        parseCheck->arrivetime = stoptimeupdatemsg.arrival.time;}
    else // use scheduled time
        {parseCheck->arrivetime = stoptimeupdatemsg.arrival.scheduled_time;}
    
    int delaytime = 0;
    if (stoptimeupdatemsg.arrival.has_delay)
        {delaytime = stoptimeupdatemsg.arrival.delay;} 

    parseCheck->arrivetime += delaytime; // arrivetime holds the number of seconds since posix time of the arrival
    time_t now;
    time(&now);
    int seconds_until = parseCheck->arrivetime - now;
    if (seconds_until < 0) {
        minutes_until = 123;   // already arrived
    } else {
        minutes_until = seconds_until / 60;
    }
    Serial.print("parse: min until:");
    Serial.println(minutes_until);

    parseCheck->found = true;
    return false;   // abort pb_decode()    
}

bool stopid_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
    Serial.println("stopid_cb()");
    char stop_id[STOP_ID_MAX];   // adjust size if needed

    size_t len = stream->bytes_left;
    if (len >= sizeof(stop_id)) len = sizeof(stop_id) - 1;

    if (!pb_read(stream, (uint8_t *)stop_id, len)) return false;
    stop_id[len] = '\0';

    Serial.printf("Stop ID: %s\n", stop_id);
    ParseState *st = (ParseState *)(*arg);
    

    if (strcmp(stop_id, "4072") != 0) { 
        st->stop_match = false; // stop not matched
        return true;   
    }
    
    st->stop_match = true;     // stop matched

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

esp_err_t http_fetch_pb(uint8_t *out_buf, int max_len, int *out_len){
    
    Serial.println("http_fetch_pb():");

    esp_http_client_config_t config = {
        .url = "https://webapps.regionofwaterloo.ca/api/grt-routes/api/tripupdates/1",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .crt_bundle_attach = esp_crt_bundle_attach,

    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        Serial.printf("HTTP request failed: %s \n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_get_content_length(client);
    int total_read = 0;

    if (content_length > max_len){
        Serial.printf("HTTP response too large: %d bytes\n", content_length);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    if (content_length > 0) {
        while (total_read < content_length && total_read < max_len) {
            // read stores length of data read in this http stream 
            // populates the buffer (out_buf and dynamically allocs memory)
            int read = esp_http_client_read(
                client,
                (char *)(out_buf + total_read),
                max_len - total_read
            );
            if (read <= 0) {
                Serial.println("Could not fetch from http.");
                return ESP_FAIL;
            }
            total_read += read;
        }
    } else {
        // Unknown content length (chunked). Read until stream ends or buffer full.
        while (total_read < max_len) {
            int read = esp_http_client_read(
                client,
                (char *)(out_buf + total_read),
                max_len - total_read
            );
            if (read <= 0) {
                break;
            }
            total_read += read;
        }
        if (total_read == 0) {
            Serial.println("HTTP body empty or read failed.");
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
    }

    *out_len = total_read;
    esp_http_client_cleanup(client);
    return ESP_OK;
}

esp_err_t checkErrors(bool status, pb_istream_t *stream){
    /* Check for errors... */
    if (!status)
    {
        Serial.printf("Decoding failed: %s\n", PB_GET_ERROR(stream));
        return ESP_FAIL;
    }
    return ESP_OK;
}