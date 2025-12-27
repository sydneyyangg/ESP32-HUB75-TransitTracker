// pb
#include "pb_encode.h"
#include "pb_decode.h"
#include "gtfs-realtime.pb.h"
// http client
#include "esp_http_client.h"
// time
#include "esp_sntp.h"
#include <time.h>
// serial
#include "Arduino.h"

#define MAX_HTTP_RECV_BUFFER 4096
#define ROUTE_ID_MAX 16
#define STOP_ID_MAX 32

uint8_t pb_buffer[MAX_HTTP_RECV_BUFFER];
int *pb_len;
bool status = false;
int minutes_until;

typedef struct {
    bool route_match;
    bool stop_match;
    bool found;
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
        if (read <= 0) {
            Serial.println("Could not fetch from http.");
            return ESP_FAIL;
        }
        total_read += read;
    }

    *out_len = total_read;
    esp_http_client_cleanup(client);
    return ESP_OK;
}

esp_err_t parse_pb(){

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
    status = pb_decode(&stream, &transit_realtime_FeedMessage_msg, &feedmsg);
    checkErrors(status, &stream);
}

bool entity_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){

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
    
    char route_id[ROUTE_ID_MAX];   // adjust size if needed

    size_t len = stream->bytes_left;
    if (len >= sizeof(route_id)) len = sizeof(route_id) - 1;

    if (!pb_read(stream, (uint8_t *)route_id, len)) return false;
    route_id[len] = '\0';

    Serial.printf("Route ID: %s\n", route_id);
    ParseState *st = (ParseState *)(*arg);

    if (strcmp(route_id, "?????") != 0) { // TWEAK THIS RMBR
        st->route_match = false; // route not matched
        return true;   
    }

    st->route_match = true;     // mark route matched

    return true;
}

bool stoptimeupdates_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
    
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
    if (seconds_until < 0) 
    minutes_until = 0;   // already arrived
    else
        minutes_until = seconds_until / 60;

    parseCheck->found = true;
    return false;   // abort pb_decode()    

    return true;
}

bool stopid_cb(pb_istream_t *stream, const pb_field_t *field, void **arg){
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

    st->stop_match = true;     // stop route matched

    return true;
}

void init_time(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
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