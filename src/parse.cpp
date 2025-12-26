#include "pb_encode.h"
#include "pb_decode.h"
#include "gtfs-realtime.pb.h"
#include "Arduino.h"

#include "esp_http_client.h"

#define MAX_HTTP_RECV_BUFFER 4096

uint8_t pb_buffer[MAX_HTTP_RECV_BUFFER];
int *pb_len;
bool status = false;
typedef struct {
    bool route_match;
    bool stop_match;
    int arrivetime;
} ParseState;

esp_err_t http_fetch_pb(uint8_t *out_buf, int max_len, int *out_len){

    esp_http_client_config_t config = {
        .url = "https://webapps.regionofwaterloo.ca/api/grt-routes/api/tripupdates/1",
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        Serial.printf("HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_get_content_length(client);
    int total_read = 0;

    while (total_read < content_length && total_read < max_len) {
        // read stores length of data read in this http stream 
        // populates the buffer (out_buf and dynamically allocs memory)
        int read = esp_http_client_read(
            client,
            (char *)(out_buf + total_read),
            max_len - total_read
        );
        if (read <= 0) break;
        total_read += read;
    }

    *out_len = total_read;
    esp_http_client_cleanup(client);
    return ESP_OK;
}

bool entity_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){

    transit_realtime_FeedEntity currententity = transit_realtime_FeedEntity_init_zero;

    // entity => tripupdate => route_id must be ... (201) or ... [later]
    currententity.trip_update.trip.route_id.funcs.decode = routeid_cb;
    currententity.trip_update.trip.route_id.arg = &state;

    currententity.trip_update.stop_time_update.funcs.decode = stoptimeupdates_cb;
    currententity.trip_update.stop_time_update.arg = &state;

    status = pb_decode(stream, &transit_realtime_FeedEntity_msg, &currententity);
    checkErrors(status, stream);

    // route_id check
    if (state == false){return false;}
    // entity must have trip update to proceed
    if (!currententity.has_trip_update){return false;}

    return true;
}

//pb_callback_t _transit_realtime_TripDescriptor::route_id
//The route_id from the GTFS that this selector refers to.
bool routeid_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
    char route_id[32];   // adjust size if needed

    // Read string safely
    if (!pb_read(stream, (uint8_t *)route_id, stream->bytes_left)) {return false;}

    route_id[stream->bytes_left] = '\0'; // null-terminate

    Serial.printf("Route ID: %s\n", route_id);
    bool *state = (bool *)(*arg);

    if (strcmp(route_id, "?????") != 0) { // TWEAK THIS RMBR
        *state = false; // route not matched
        return true;   
    }

    *state = true;     // mark route matched

    return true;
}

bool stoptimeupdates_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
    transit_realtime_TripUpdate_StopTimeUpdate stoptimeupdatemsg = transit_realtime_TripUpdate_StopTimeUpdate_init_zero;
    
    stoptimeupdatemsg.stop_id.funcs.decode = stopid_cb;
    stoptimeupdatemsg.stop_id.arg = &state;

    status = pb_decode(stream, &transit_realtime_TripUpdate_StopTimeUpdate_msg, &stoptimeupdatemsg);
    checkErrors(status, stream);

    // quit if does not have arrival update
    if (!stoptimeupdatemsg.has_arrival) {return false;}
    // quit if stop_id is not desired stop
    if (state == false){return false;}

    // use given time if provided
    if (stoptimeupdatemsg.arrival.has_time)
        {state.arrivetime = stoptimeupdatemsg.arrival.time;}
    else // use scheduled time
        {state.arrivetime = stoptimeupdatemsg.arrival.scheduled_time;}
    
    int delaytime = 0;
    if (stoptimeupdatemsg.arrival.has_delay)
        {delaytime = stoptimeupdatemsg.arrival.delay;}

    state.arrivetime += delaytime;

    return true;
}

bool stopid_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
    char stop_id[32];   // adjust size if needed

    // Read string safely
    if (!pb_read(stream, (uint8_t *)stop_id, stream->bytes_left)) {return false;}

    stop_id[stream->bytes_left] = '\0'; // null-terminate

    Serial.printf("Stop ID: %s\n", stop_id);
    bool *state = (bool *)(*arg);
    

    if (strcmp(stop_id, "4072") != 0) { 
        *state = false; // stop not matched
        return true;   
    }

    *state = true;     // stop route matched

    return true;
}


esp_err_t parse_pb(int *out_eta){

    if(http_fetch_pb(pb_buffer, MAX_HTTP_RECV_BUFFER, pb_len) == ESP_OK){
        Serial.printf("Fetched %d bytes", *pb_len);
    }

    // make the buffer a pb stream
    pb_istream_t stream = pb_istream_from_buffer(pb_buffer, size_t(*pb_len));

    // first, configure feedMsg
     // feedmsg.entity is a callback hook, not a data retrieval
    // decode is a function executed when entity is found
    // arg is an argument for that function 
    transit_realtime_FeedMessage feedmsg = transit_realtime_FeedMessage_init_zero;
    ParseState state = {0};
    feedmsg.entity.arg = &state;  
    feedmsg.entity.funcs.decode = entity_cb;
    if (!feedmsg.entity.funcs.decode) {return;}
    status = pb_decode(&stream, &transit_realtime_FeedMessage_msg, &feedmsg);
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