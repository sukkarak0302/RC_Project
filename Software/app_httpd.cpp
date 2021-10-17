// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "camera_index.h"
#include "Arduino.h"

#include "fb_gfx.h"
#include "fd_forward.h"
#include "fr_forward.h"

#include "RC_Control.h"

#define ENROLL_CONFIRM_TIMES 5
#define FACE_ID_SAVE_NUMBER 7

#define FACE_COLOR_WHITE  0x00FFFFFF
#define FACE_COLOR_BLACK  0x00000000
#define FACE_COLOR_RED    0x000000FF
#define FACE_COLOR_GREEN  0x0000FF00
#define FACE_COLOR_BLUE   0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN   (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)

typedef struct {
        size_t size; //number of values used for filtering
        size_t index; //current value index
        size_t count; //value count
        int sum;
        int * values; //array to be filled with values
} ra_filter_t;

typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

const char* get_HTML();

static ra_filter_t ra_filter;
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;
httpd_handle_t distance_httpd = NULL;

static mtmn_config_t mtmn_config = {0};
static int8_t detection_enabled = 0;
static int8_t recognition_enabled = 0;
static int8_t is_enrolling = 0;
static face_id_list id_list = {0};

static int ACCVal = 5;
static int STRVal = 5;
static int DISTVal = 0;
static int preDISTVal = 10;
static int DistMEM_writing = 0;

esp_err_t camera_init();
int get_DistVal();



static ra_filter_t * ra_filter_init(ra_filter_t * filter, size_t sample_size){
    memset(filter, 0, sizeof(ra_filter_t));

    filter->values = (int *)malloc(sample_size * sizeof(int));
    if(!filter->values){
        return NULL;
    }
    memset(filter->values, 0, sample_size * sizeof(int));

    filter->size = sample_size;
    return filter;
}

static int ra_filter_run(ra_filter_t * filter, int value){
    if(!filter->values){
        return value;
    }
    filter->sum -= filter->values[filter->index];
    filter->values[filter->index] = value;
    filter->sum += filter->values[filter->index];
    filter->index++;
    filter->index = filter->index % filter->size;
    if (filter->count < filter->size) {
        filter->count++;
    }
    return filter->sum / filter->count;
}

static void rgb_print(dl_matrix3du_t *image_matrix, uint32_t color, const char * str){
    fb_data_t fb;
    fb.width = image_matrix->w;
    fb.height = image_matrix->h;
    fb.data = image_matrix->item;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR888;
    fb_gfx_print(&fb, (fb.width - (strlen(str) * 14)) / 2, 10, color, str);
}

static int rgb_printf(dl_matrix3du_t *image_matrix, uint32_t color, const char *format, ...){
    char loc_buf[64];
    char * temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if(len >= sizeof(loc_buf)){
        temp = (char*)malloc(len+1);
        if(temp == NULL) {
            return 0;
        }
    }
    vsnprintf(temp, len+1, format, arg);
    va_end(arg);
    rgb_print(image_matrix, color, temp);
    if(len > 64){
        free(temp);
    }
    return len;
}

static void draw_face_boxes(dl_matrix3du_t *image_matrix, box_array_t *boxes, int face_id){
    int x, y, w, h, i;
    uint32_t color = FACE_COLOR_YELLOW;
    if(face_id < 0){
        color = FACE_COLOR_RED;
    } else if(face_id > 0){
        color = FACE_COLOR_GREEN;
    }
    fb_data_t fb;
    fb.width = image_matrix->w;
    fb.height = image_matrix->h;
    fb.data = image_matrix->item;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR888;
    for (i = 0; i < boxes->len; i++){
        // rectangle box
        x = (int)boxes->box[i].box_p[0];
        y = (int)boxes->box[i].box_p[1];
        w = (int)boxes->box[i].box_p[2] - x + 1;
        h = (int)boxes->box[i].box_p[3] - y + 1;
        fb_gfx_drawFastHLine(&fb, x, y, w, color);
        fb_gfx_drawFastHLine(&fb, x, y+h-1, w, color);
        fb_gfx_drawFastVLine(&fb, x, y, h, color);
        fb_gfx_drawFastVLine(&fb, x+w-1, y, h, color);
#if 0
        // landmark
        int x0, y0, j;
        for (j = 0; j < 10; j+=2) {
            x0 = (int)boxes->landmark[i].landmark_p[j];
            y0 = (int)boxes->landmark[i].landmark_p[j+1];
            fb_gfx_fillRect(&fb, x0, y0, 3, 3, color);
        }
#endif
    }
}

static int run_face_recognition(dl_matrix3du_t *image_matrix, box_array_t *net_boxes){
    dl_matrix3du_t *aligned_face = NULL;
    int matched_id = 0;

    aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
    if(!aligned_face){
        Serial.println("Could not allocate face recognition buffer");
        return matched_id;
    }
    if (align_face(net_boxes, image_matrix, aligned_face) == ESP_OK){
        if (is_enrolling == 1){
            int8_t left_sample_face = enroll_face(&id_list, aligned_face);

            if(left_sample_face == (ENROLL_CONFIRM_TIMES - 1)){
                Serial.printf("Enrolling Face ID: %d\n", id_list.tail);
            }
            Serial.printf("Enrolling Face ID: %d sample %d\n", id_list.tail, ENROLL_CONFIRM_TIMES - left_sample_face);
            rgb_printf(image_matrix, FACE_COLOR_CYAN, "ID[%u] Sample[%u]", id_list.tail, ENROLL_CONFIRM_TIMES - left_sample_face);
            if (left_sample_face == 0){
                is_enrolling = 0;
                Serial.printf("Enrolled Face ID: %d\n", id_list.tail);
            }
        } else {
            matched_id = recognize_face(&id_list, aligned_face);
            if (matched_id >= 0) {
                Serial.printf("Match Face ID: %u\n", matched_id);
                rgb_printf(image_matrix, FACE_COLOR_GREEN, "Hello Subject %u", matched_id);
            } else {
                Serial.println("No Match Found");
                rgb_print(image_matrix, FACE_COLOR_RED, "Intruder Alert!");
                matched_id = -1;
            }
        }
    } else {
        Serial.println("Face Not Aligned");
        //rgb_print(image_matrix, FACE_COLOR_YELLOW, "Human Detected");
    }

    dl_matrix3du_free(aligned_face);
    return matched_id;
}

static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t out_len, out_width, out_height;
    uint8_t * out_buf;
    bool s;
    bool detected = false;
    int face_id = 0;
    if(!detection_enabled || fb->width > 400){
        size_t fb_len = 0;
        if(fb->format == PIXFORMAT_JPEG){
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
        esp_camera_fb_return(fb);
        int64_t fr_end = esp_timer_get_time();
        Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
        return res;
    }

    dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
    if (!image_matrix) {
        esp_camera_fb_return(fb);
        Serial.println("dl_matrix3du_alloc failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    out_buf = image_matrix->item;
    out_len = fb->width * fb->height * 3;
    out_width = fb->width;
    out_height = fb->height;

    s = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
    esp_camera_fb_return(fb);
    if(!s){
        dl_matrix3du_free(image_matrix);
        Serial.println("to rgb888 failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    box_array_t *net_boxes = face_detect(image_matrix, &mtmn_config);

    if (net_boxes){
        detected = true;
        if(recognition_enabled){
            face_id = run_face_recognition(image_matrix, net_boxes);
        }
        draw_face_boxes(image_matrix, net_boxes, face_id);
        free(net_boxes->score);
        free(net_boxes->box);
        free(net_boxes->landmark);
        free(net_boxes);
    }

    jpg_chunking_t jchunk = {req, 0};
    s = fmt2jpg_cb(out_buf, out_len, out_width, out_height, PIXFORMAT_RGB888, 90, jpg_encode_stream, &jchunk);
    dl_matrix3du_free(image_matrix);
    if(!s){
        Serial.println("JPEG compression failed");
        return ESP_FAIL;
    }

    int64_t fr_end = esp_timer_get_time();
    Serial.printf("FACE: %uB %ums %s%d\n", (uint32_t)(jchunk.len), (uint32_t)((fr_end - fr_start)/1000), detected?"DETECTED ":"", face_id);
    return res;
}

static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];
    dl_matrix3du_t *image_matrix = NULL;
    bool detected = false;
    int face_id = 0;
    int64_t fr_start = 0;
    int64_t fr_ready = 0;
    int64_t fr_face = 0;
    int64_t fr_recognize = 0;
    int64_t fr_encode = 0;

    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while(true){
        detected = false;
        face_id = 0;
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
            fr_start = esp_timer_get_time();
            fr_ready = fr_start;
            fr_face = fr_start;
            fr_encode = fr_start;
            fr_recognize = fr_start;
            if(!detection_enabled || fb->width > 400){
                if(fb->format != PIXFORMAT_JPEG){
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if(!jpeg_converted){
                        Serial.println("JPEG compression failed");
                        res = ESP_FAIL;
                    }
                } else {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
            } else {

                image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);

                if (!image_matrix) {
                    Serial.println("dl_matrix3du_alloc failed");
                    res = ESP_FAIL;
                } else {
                    if(!fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item)){
                        Serial.println("fmt2rgb888 failed");
                        res = ESP_FAIL;
                    } else {
                        fr_ready = esp_timer_get_time();
                        box_array_t *net_boxes = NULL;
                        if(detection_enabled){
                            net_boxes = face_detect(image_matrix, &mtmn_config);
                        }
                        fr_face = esp_timer_get_time();
                        fr_recognize = fr_face;
                        if (net_boxes || fb->format != PIXFORMAT_JPEG){
                            if(net_boxes){
                                detected = true;
                                if(recognition_enabled){
                                    face_id = run_face_recognition(image_matrix, net_boxes);
                                }
                                fr_recognize = esp_timer_get_time();
                                draw_face_boxes(image_matrix, net_boxes, face_id);
                                free(net_boxes->score);
                                free(net_boxes->box);
                                free(net_boxes->landmark);
                                free(net_boxes);
                            }
                            if(!fmt2jpg(image_matrix->item, fb->width*fb->height*3, fb->width, fb->height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len)){
                                Serial.println("fmt2jpg failed");
                                res = ESP_FAIL;
                            }
                            esp_camera_fb_return(fb);
                            fb = NULL;
                        } else {
                            _jpg_buf = fb->buf;
                            _jpg_buf_len = fb->len;
                        }
                        fr_encode = esp_timer_get_time();
                    }
                    dl_matrix3du_free(image_matrix);
                }
            }
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();

        int64_t ready_time = (fr_ready - fr_start)/1000;
        int64_t face_time = (fr_face - fr_ready)/1000;
        int64_t recognize_time = (fr_recognize - fr_face)/1000;
        int64_t encode_time = (fr_encode - fr_recognize)/1000;
        int64_t process_time = (fr_encode - fr_start)/1000;
        
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
        Serial.printf("MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps), %u+%u+%u+%u=%u %s%d\n",
            (uint32_t)(_jpg_buf_len),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
            avg_frame_time, 1000.0 / avg_frame_time,
            (uint32_t)ready_time, (uint32_t)face_time, (uint32_t)recognize_time, (uint32_t)encode_time, (uint32_t)process_time,
            (detected)?"DETECTED ":"", face_id
        );
    }

    last_frame = 0;
    return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
    char*  buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    sensor_t * s = esp_camera_sensor_get();
    int res = 0;

    if(!strcmp(variable, "framesize")) {
        if(s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
    }
    else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
    else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
    else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
    else if(!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
    else if(!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
    else if(!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
    else if(!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
    else if(!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
    else if(!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
    else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
    else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
    else if(!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
    else if(!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
    else if(!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
    else if(!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
    else if(!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
    else if(!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
    else if(!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
    else if(!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
    else if(!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
    else if(!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
    else if(!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
    else if(!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
    else if(!strcmp(variable, "face_detect")) {
        detection_enabled = val;
        if(!detection_enabled) {
            recognition_enabled = 0;
        }
    }
    else if(!strcmp(variable, "face_enroll")) is_enrolling = val;
    else if(!strcmp(variable, "face_recognize")) {
        recognition_enabled = val;
        if(recognition_enabled){
            detection_enabled = val;
        }
    }
    else {
        res = -1;
    }

    if(res){
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];

    sensor_t * s = esp_camera_sensor_get();
    char * p = json_response;
    *p++ = '{';

    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    p+=sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p+=sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p+=sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p+=sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p+=sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p+=sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p+=sprintf(p, "\"awb\":%u,", s->status.awb);
    p+=sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p+=sprintf(p, "\"aec\":%u,", s->status.aec);
    p+=sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p+=sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p+=sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p+=sprintf(p, "\"agc\":%u,", s->status.agc);
    p+=sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p+=sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p+=sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p+=sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p+=sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p+=sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p+=sprintf(p, "\"vflip\":%u,", s->status.vflip);
    p+=sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p+=sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p+=sprintf(p, "\"colorbar\":%u,", s->status.colorbar);
    p+=sprintf(p, "\"face_detect\":%u,", detection_enabled);
    p+=sprintf(p, "\"face_enroll\":%u,", is_enrolling);
    p+=sprintf(p, "\"face_recognize\":%u", recognition_enabled);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    sensor_t * s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        return httpd_resp_send(req, (const char *)index_ov3660_html_gz, index_ov3660_html_gz_len);
    }
    return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
}

void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };


    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };

   httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };


    ra_filter_init(&ra_filter, 20);
    
    mtmn_config.type = FAST;
    mtmn_config.min_face = 80;
    mtmn_config.pyramid = 0.707;
    mtmn_config.pyramid_times = 4;
    mtmn_config.p_threshold.score = 0.6;
    mtmn_config.p_threshold.nms = 0.7;
    mtmn_config.p_threshold.candidate_number = 20;
    mtmn_config.r_threshold.score = 0.7;
    mtmn_config.r_threshold.nms = 0.7;
    mtmn_config.r_threshold.candidate_number = 10;
    mtmn_config.o_threshold.score = 0.7;
    mtmn_config.o_threshold.nms = 0.7;
    mtmn_config.o_threshold.candidate_number = 1;
    
    face_id_init(&id_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
    
    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}

static esp_err_t HTML_Index(httpd_req_t *req)
{

	//char* result = "<html>Hello World!!</html>";
	//char* result = "<h1>HEEEEEEEELLLLOOOOOOUUUUAJSDIJFIZNXICVIAJSIOJEFIOJSOIDJF</h1>";
	//const char *p = result;
	esp_err_t res = ESP_OK;
	sensor_t * s = esp_camera_sensor_get();


    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "UTF-8");

    //Serial.printf("HTML test ok\n");

	res = httpd_resp_send(req, get_HTML(), strlen(get_HTML()));

	Serial.printf("HTML test ok2\n");

	return res;

}

static esp_err_t control_handler(httpd_req_t *req){

	char QueryBuf[12];
	size_t QueryBuf_len;
	char ACCVal_c[2], STRVal_c[2];
	char* ACC = "ACC";
	char* STR = "STR";
	esp_err_t res;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "UTF-8");

	QueryBuf_len = httpd_req_get_url_query_len(req) + 1;
	//QueryBuf = malloc(QueryBuf_len);
	Serial.printf("Control Called");
	if ( QueryBuf_len > 0 )
	{
		Serial.printf("QueryBuf_len : %d ", QueryBuf_len);
		//Serial.printf("MainRun ");
		res = httpd_req_get_url_query_str(req, QueryBuf, QueryBuf_len );
		Serial.printf("%d", res);
		Serial.printf(" %s\n", QueryBuf);
		if( httpd_req_get_url_query_str(req, QueryBuf, QueryBuf_len ) == ESP_OK )
		{
			Serial.printf("QueryBuf : %s", QueryBuf);
			if( httpd_query_key_value(QueryBuf, "ACC" , ACCVal_c, sizeof(ACCVal_c)) == ESP_OK )
			{
				//Serial.printf("RAW_ACC : %s", ACCVal_c);
				ACCVal = (int)(ACCVal_c[0] - '0');
				Serial.printf("ACC : %d", ACCVal);
			}
			if( httpd_query_key_value(QueryBuf, "STR" , STRVal_c, sizeof(STRVal_c)) == ESP_OK )
			{
				//Serial.printf("RAW_STR : %s ", STRVal_c);
				STRVal = (int)(STRVal_c[0] - '0');
				Serial.printf("STR : %d\n", STRVal);
			}
		}
		else
		{
			Serial.printf("Query String NotFound\n");
		}
	}
	httpd_resp_send(req, NULL, 0);
	Serial.print("Return!!");
	return res;
}

static esp_err_t test_handler(httpd_req_t *req)
{
	char* result = "<h5>AA</h5>";
	esp_err_t res = ESP_OK;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "UTF-8");

    Serial.printf("HTML test ok2\n");
	res = httpd_resp_send_chunk(req, result, strlen(result));
    Serial.printf(result);
	Serial.printf("HTML test ok3\n");

	return res;
}


static esp_err_t streaming_handle(httpd_req_t *req)
{
	camera_fb_t * fb = NULL;
	esp_err_t res = ESP_OK;
	size_t _jpg_buf_len = 0;
	uint8_t * _jpg_buf = NULL;
	char * part_buf[64];

	sensor_t * s = esp_camera_sensor_get();
	s->set_vflip(s, 1);
	s->set_hmirror(s, 1);

	res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	Serial.println("STREAMING Handler Started!");
    while(true)
    {
    	//Serial.println("Working!!");
        fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        }
        else
        {
                if(fb->format != PIXFORMAT_JPEG)
                {
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if(!jpeg_converted)
                    {
                        Serial.println("JPEG compression failed");
                        res = ESP_FAIL;
                    }
                }
                else
                {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
        }

        if(res == ESP_OK)
        {
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        if(fb)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if(_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK)
        {
            break;
        }
    }

	return res;
}

esp_err_t camera_capture( httpd_req_t *req ){
    //acquire a frame
    camera_fb_t * fb;
    esp_err_t res = ESP_OK;
    size_t fb_len = 0;

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    //replace this with your own function
    //process_image(fb->width, fb->height, fb->format, fb->buf, fb->len);
    Serial.printf("camera capture");

    if(res == ESP_OK){
        if(fb->format == PIXFORMAT_JPEG){
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
    }
    //return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);
    return res;
}

esp_err_t distance_handler( httpd_req_t *req ){

	char* result;
	char intData[4]= "abc";
	char intData2[5];
	esp_err_t res = ESP_OK;
	int TempDistVal;


    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "UTF-8");

    		if( DISTVal <= 300 )
    		{
    			if(DISTVal >= 1000) intData2[0] = (char)( DISTVal / 1000 ) + '0'; //(char)((DISTVal - DISTVal / 100 - DISTVal / 10 - DISTVal)/1000 + '0');
    			else intData2[0] = '0';

    			if(DISTVal >= 100) intData2[1] = (char)( ( DISTVal % 1000 - DISTVal % 100 ) / 100 ) + '0'; //DISTVal - ( DISTVal / 1000 ) * 1000 - DISTVal % 100 ) + '0'; //(char)((DISTVal - DISTVal / 1000 - DISTVal / 10 - DISTVal)/100 + '0');
    			else intData2[1] = '0';

    			if(DISTVal >= 10) intData2[2] = (char)( ( DISTVal % 100 - DISTVal % 10 ) /10 ) + '0'; //(char)((DISTVal - DISTVal / 1000 - DISTVal / 100 - DISTVal)/10 + '0');
    			else intData2[2] = '0';

				intData2[3] = (char)( DISTVal - ( DISTVal / 10 ) * 10 ) + '0'; // (char)((DISTVal - DISTVal / 1000 - DISTVal / 100 - DISTVal / 10+ '0'));
				Serial.printf("char : %s, int : %d\n", intData2, DISTVal );
    		}
    		else
    		{
    			intData2[0] = '0';
    			intData2[1] = '0';
    			intData2[2] = '0';
    			intData2[3] = '0';
    			intData2[4] = '\0';
    		}
			res = httpd_resp_send(req, intData2, strlen(intData2));

	return res;
}

void startCameraServer_Core1(){

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = HTML_Index,
        .user_ctx  = NULL
    };

    httpd_uri_t control_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = control_handler,
        .user_ctx  = NULL
    };

	Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index);
        httpd_register_uri_handler(camera_httpd, &control_uri);
        Serial.printf("index configuration completed");
    }

}

void startCameraServer_Sens()
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	httpd_uri_t distance_uri = {
	    .uri       = "/distance",
	    .method    = HTTP_GET,
	    .handler   = distance_handler,
	    .user_ctx  = NULL
	};

    config.server_port += 2;
    config.ctrl_port += 2;
    Serial.printf("Starting distance server on port: '%d'\n", config.server_port);
    if (httpd_start(&distance_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(distance_httpd, &distance_uri);
        Serial.printf("Distance Configuration completed\n");
    }
    else
    {
    	Serial.printf("SENS Failed\n");
    }

}

void startCameraServer_Both()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = HTML_Index,
        .user_ctx  = NULL
    };

    httpd_uri_t control_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = control_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t streaming_uri = {
        .uri       = "/streaming",
        .method    = HTTP_GET,
        .handler   = streaming_handle,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };

	httpd_uri_t distance_uri = {
	    .uri       = "/distance",
	    .method    = HTTP_GET,
	    .handler   = distance_handler,
	    .user_ctx  = NULL
	};


	Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index);
        httpd_register_uri_handler(camera_httpd, &control_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
        httpd_register_uri_handler(camera_httpd, &distance_uri);
        Serial.printf("index configuration completed");
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &streaming_uri);
        //httpd_register_uri_handler(stream_httpd, &distance_uri);
        Serial.printf("CAM Configuration completed");
    }


    config.server_port += 1;
    //config.ctrl_port += 1;
    Serial.printf("Starting distance server on port: '%d'\n", config.server_port);
    if (httpd_start(&distance_httpd, &config) == ESP_OK)
    {
    	httpd_register_uri_handler(distance_httpd, &distance_uri);
    	Serial.printf("distance configuration completed");
    }

}

const char* get_HTML()
{
	const char* page3 = "\
	<style>\
	input[type=range][orient=vertical]\
	{\
		writing-mode: bt-lr; /* IE */\
		-webkit-appearance: slider-vertical; /* WebKit */\
		width: 8px;\
		height: 100px;\
		border: 30px;\
		padding: 0 5px;\
	}\
	</style>\
	\
	\
	<head>\
	<script>\
	var url = location.href;\
	var ACCVal = 0;\
	var STRVal = 0;\
	var DISTVal = 0;\
	function Func_Acc(){\
	  var xhttp = new XMLHttpRequest();\
	  var ACCVal = document.getElementById(\"Accelerator\").value;\
	  document.getElementById(\"Acc\").innerHTML = ACCVal;\
	  var url2 = url + \"control?ACC=\" + ACCVal + \"&STR=\" + STRVal;\
	  xhttp.open(\"GET\", url2, true);\
	  xhttp.send();\
	}\
	\
	function Func_Str(){\
	  var xhttp = new XMLHttpRequest();\
	  var STRVal = document.getElementById(\"Steering\").value;\
	  document.getElementById(\"Str\").innerHTML = STRVal;\
	  var url2 = url + \"control?ACC=\" + ACCVal + \"&STR=\" + STRVal;\
	  xhttp.open(\"GET\", url2, true);\
	  xhttp.send();\
	}\
	function Dist_Rep(){\
      const timer = setInterval( () => { Func_Dist(); }, 200 );\
	}\
	function Func_Dist(){\
	  var xhttp = new XMLHttpRequest();\
	  xhttp.onreadystatechange = function(){\
	  if (xhttp.readyState === xhttp.DONE) {\
		console.log(xhttp.responseText);\
		DISTVal = xhttp.responseText; \
		document.getElementById(\"RearDist\").innerHTML = DISTVal;\
	  }\
	  };\
	  var url2 = url + \"distance\";\
	  xhttp.open(\"GET\", url2, true);\
	  xhttp.send();\
	  \
	}\
	document.addEventListener(\"DOMContentLoaded\", function(){ Dist_Rep(); });\
	</script>\
	</head>\
	\
	<body>\
	<h1> QSLA MOdEL 3.14 </h1><br>\
	<table border = 0 width = \"100%\">\
	<tr>\
	<td width = 200>\
	<form>\
	<input type=\"range\" name=\"slider\" value=\"5\" min=\"1\" max=\"9\" id=\"Accelerator\" oninput=\"Func_Acc()\" orient=\"vertical\"><br>\
	</form>\
	</td>\
	<td>\
	<iframe src=\"http://192.168.4.1:81/streaming\" width=320 height=240></iframe>\
	</td>\
	<td>\
	<input type=\"range\" name=\"slider\" value=\"5\" min=\"1\" max=\"9\" id=\"Steering\" oninput=\"Func_Str()\">\
	</td>\
	</tr>\
	<tr>\
	<td>\
	</td>\
	<td>\
	<h4>Rear Object Distance : <span id=\"RearDist\"></span> cm </h4>\
	</td>\
	<td>\
	</td>\
	</tr>\
	</table>\
	\
	<h4>Current Accelerator : <span id=\"Acc\"></span> / Current Steering : <span id=\"Str\"></span></h4>\
	</body>\
	";

	const char * page4 ="\
				<style>\
	input[type=range][orient=vertical] {\
		-webkit-appearance: none;\
		/*-webkit-appearance: slider-vertical; /*if we remove this slider-vertical then horizondally range bar styles applies correctly*/\
		width: 200px;\
		height: 0px;\
		transform: rotate(-90deg);\
		transform-origin:bottom;\
	}\
	\
	input[type=range]::-webkit-slider-thumb {\
		-webkit-appearance: none;\
		border: none;\
		height: 60px;\
		width: 60px;\
		border-radius: 10%;\
		background: black;\
		margin-top: 0px;\
	}\
\
	input[type=range]::-webkit-slider-runnable-track {\
		background: white;  \
		border: 2px solid black; \
	}\
\
	input[type=range][orient=horizontal] {\
		-webkit-appearance: none;\
		/*-webkit-appearance: slider-vertical; /*if we remove this slider-vertical then horizondally range bar styles applies correctly*/\
		width: 200px;\
		height: 0px;\
		transform-origin:bottom;  \
	}\
	</style>\
	\
	\
	<head>\
	<script>\
	var url = location.href;\
	var ACCVal = 5;\
	var STRVal = 5;\
	var DISTVal = 0;\
	\
	function Init(){\
	  document.getElementById(\"Acc\").innerHTML = ACCVal;\
	  document.getElementById(\"Str\").innerHTML = STRVal;\
	}\
	function Func_Acc_Bar(){\
	  ACCVal = document.getElementById(\"Accelerator\").value;\
	  Func_Output(ACCVal, STRVal);\
	}\
	\
	function Func_Str_Bar(){\
	  STRVal = document.getElementById(\"Steering\").value;\
	  Func_Output(ACCVal, STRVal);\
	}\
	\
	\
	function Func_Output(ACC, STR){\
	  var xhttp = new XMLHttpRequest();\
	  document.getElementById(\"Acc\").innerHTML = ACC;\
	  document.getElementById(\"Str\").innerHTML = STR;\
	  Func_Color(ACC,STR);\
	  var url2 = url + \"control?ACC=\" + ACC + \"&STR=\" + STR;\
	  xhttp.open(\"GET\", url2, true);\
	  xhttp.send();\
	}\
	\
	function Func_Color(ACC, STR){\
	  var ACC_Color1 = 0;\
	  var ACC_Color = 0;\
	  var STR_Color1 = 0;\
	  var STR_Color = 0;\
	  \
	  if(ACC > 5) { \
		ACC_Color1 = parseInt(255 - 255 / 4 * ( ACC - 5 )); \
		ACC_Color = '#'+ACC_Color1.toString(16)+'0000';\
	  }\
	  else if (ACC == 5) { ACC_Color = '#FFFFFF'; }\
	  else { \
		ACC_Color1 = parseInt(255 - 255 / 4 * ( 5 - ACC )); \
		ACC_Color = '#0000'+ACC_Color1.toString(16);\
	  }\
\
	  if(STR > 5) { \
		STR_Color1 = parseInt(255 - 255 / 4 * ( STR - 5 )); \
		STR_Color = '#'+STR_Color1.toString(16)+'0000';\
	  }\
	  else if (STR == 5) { STR_Color = '#FFFFFF'; }\
	  else { \
		STR_Color1 = parseInt(255 - 255 / 4 * ( 5 - STR )); \
		STR_Color = '#0000'+STR_Color1.toString(16);\
	  }\
	  console.log(\"STR : \"+ STR_Color+\" ACC : \" + ACC_Color);\
	  document.getElementById(\"ACC_Color\").style.backgroundColor = ACC_Color;\
	  document.getElementById(\"STR_Color\").style.backgroundColor = STR_Color;\
	}\
	\
	function Dist_Rep(){\
      const timer = setInterval( () => { Func_Dist(); }, 250 );\
	}\
	\
	function Func_Dist(){\
	  var xhttp = new XMLHttpRequest();\
	  xhttp.onreadystatechange = function(){\
	  if (xhttp.readyState === xhttp.DONE) {\
		console.log(xhttp.responseText);\
		DISTVal = xhttp.responseText; \
		document.getElementById(\"RearDist\").innerHTML = DISTVal;\
	  }\
	  };\
	  var url2 = url + \"distance\";\
	  xhttp.open(\"GET\", url2, true);\
	  xhttp.send(); \
	}\
	\
	function Func_KeyPress(){\
		var keycode=event.keyCode;\
		console.log(\"Key code : \" + keycode);\
		if(keycode == 37) /*Left*/\
		{\
			if ( STRVal >= 2 )\
			{\
				STRVal = Number(STRVal)-1;\
			}\
		}\
		if(keycode == 38) /*Up*/\
		{\
			if ( ACCVal <= 8 )\
			{\
				ACCVal = Number(ACCVal)+1 ;\
			}\
		}\
		if(keycode == 39) /*Right*/\
		{\
			if ( STRVal <= 8 )\
			{\
				STRVal = Number(STRVal) + 1;\
			}\
		}\
		if(keycode == 40) /*Down*/ \
		{\
			if ( ACCVal >= 2 )\
			{\
				ACCVal = Number(ACCVal)-1 ;\
			}\
		}\
		document.getElementById(\"Str\").innerHTML = STRVal;\
		document.getElementById(\"Steering\").innerHTML = STRVal;\
		document.getElementById(\"Acc\").innerHTML = ACCVal;\
		document.getElementById(\"Accelerator\").innerHTML = ACCVal;\
		STRVal_Pre = STRVal;\
		ACCVal_Pre = ACCVal;\
		console.log(\"ACCVal : \"+ ACCVal +\"STRVal : \"+STRVal);\
		Func_Output(ACCVal, STRVal);\
	}\
	\
	document.addEventListener(\"keydown\", function(){Func_KeyPress();} );\
	document.addEventListener(\"DOMContentLoaded\", function(){ Dist_Rep(); });\
	</script>\
	</head>\
	\
	<body style = \"background-color:#B2CCFF;\" onload = \"Init()\">\
	<div align = center><h5> Test Version 2021.01.09 </h5></div><br>\
	<table border =0 width = \"100%\">\
	<tr>\
	<td width = 100 id = \"ACC_Color\" style = \"background-color:#FFFFFF;\">\
	<form>\
	<div align = \"center\"><input type=\"range\" name=\"slider\" value=\"5\" min=\"1\" max=\"9\" id=\"Accelerator\" oninput=\"Func_Acc_Bar()\" orient=\"vertical\"></div>\
	</form>\
	</td>\
	<td width = 20>\
	<font color = red>¡ã<br>F<br>W<br>¡ã<br>¡ã</font><br><font color = blue> <br>¡å<br>¡å<br>B<br>W<br>¡å</font>\
	</td>\
	<td>\
	<div align = center><iframe src=\"http://192.168.4.1:81/streaming\" width=320 height=240></iframe></div>\
	</td>\
	<td width = 100 id = \"STR_Color\" style = \"background-color:#FFFFFF;\">\
	<div align = \"center\"><input type=\"range\" name=\"slider\" value=\"5\" min=\"1\" max=\"9\" id=\"Steering\" oninput=\"Func_Str_Bar()\" orient=\"horizontal\"></div>\
	</td>\
	</tr>\
	<tr>\
	<td>\
	</td>\
	<td>\
	</td>\
	<td>\
	<div align = \"center\"><h4>Rear Object Distance : <span id=\"RearDist\"></span> cm </h4></div>\
	</td>\
	<td>\
	<font color = blue>¢¸LEFT¢¸¢¸¢¸¢¸</font><font color = red> ¢º¢º¢º¢ºRIGHT¢º</font>\
	</td>\
	</tr>\
	</table>\
	<div align=center><h5>Current Accelerator : <span id=\"Acc\"></span> / Current Steering : <span id=\"Str\"></span></h5></div>\
	</body>";

	return page4;
}

int get_ACCVal()
{
	return ACCVal;
}

int get_STRVal()
{
	return STRVal;
}

void set_DistVal(int val)
{
	DistMEM_writing = 1;
	DISTVal = val;
	DistMEM_writing = 0;
}

int get_DistVal()
{
	return DISTVal;
}
