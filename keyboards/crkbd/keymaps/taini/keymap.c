#include QMK_KEYBOARD_H
#include "bootloader.h"
#ifdef PROTOCOL_LUFA
  #include "lufa.h"
  #include "split_util.h"
#endif
#include "ssd1306.h"
#include <math.h>
#include <print.h>

extern keymap_config_t keymap_config;

static uint32_t oled_timer = 0;
extern uint8_t is_master;

// Each layer gets a name for readability, which is then used in the keymap matrix below.
// The underscores don't mean anything - you can have a layer called STUFF or any other name.
// Layer names don't all need to be of the same length, obviously, and you can also skip them
// entirely and just use numbers.
#define _QWERTY 0
#define _LOWER 1
#define _RAISE 2
#define _ADJUST 3

enum custom_keycodes {
  QWERTY = SAFE_RANGE,
  LOWER,
  RAISE,
  ADJUST,
  BACKLIT,
  RGBRST
};


#define KC______ KC_TRNS
#define KC_XXXXX KC_NO
#define KC_LOWER LOWER
#define KC_RAISE RAISE
#define KC_RST   RESET

static uint32_t kpm_timer_master = 0;
static uint32_t kpm_timer_slave = 0;
static uint32_t kpm_timer_frame = 0;
static uint16_t frame_value = 0;

const static uint8_t DIGIT_SIZE = 128;
const static uint8_t DIGIT_OFFSET = 32;
const static uint16_t MAXIMUM_TIME = 60000;
const static uint16_t FRAME_UPDATE = 100;

typedef struct node {
    uint16_t time_diff;
    struct node * next;
} time_node;

typedef struct list {
	time_node* head;
    time_node* tail;
    int length;
    uint16_t time_total;
} time_list;

time_list master_list = {NULL, NULL, 0, 0};
time_list slave_list = {NULL, NULL, 0, 0};

void remove_time_node(time_list* list) {
    if (list->length == 0) {
        return;
    }

    time_node* tmp = list->tail;

    if (list->length == 1) {
        list->tail = NULL;
        list->head = NULL;
        list->length = 0;
        list->time_total = 0;
    } else {
        list->length -= 1;
        list->time_total -= list->tail->next->time_diff;
        list->tail = list->tail->next;
    }
    free(tmp);
}

void check_time_list(time_list* list, uint32_t* kpm_timer) {
    // uprintf("%5u - %5u %5u\n", list->tail->time_diff, list->time_total, timer_elapsed(*kpm_timer));
    while (list->length > 0 && list->time_total + timer_elapsed(*kpm_timer) >= MAXIMUM_TIME) {
        remove_time_node(list);
    }
}

void add_time_node(time_list* list, uint32_t* kpm_timer) {
    check_time_list(list, kpm_timer);
    uint16_t time_value = timer_elapsed(*kpm_timer);
    *kpm_timer = timer_read();

    if (list->head == NULL) {
        list->head = (time_node*) malloc(sizeof(time_node));
        list->tail = list->head;
        time_value = 0;
    } else {
        list->head->next = (time_node*) malloc(sizeof(time_node));
        list->head = list->head->next;
    }
    list->head->time_diff = time_value;
    list->head->next = NULL;

    list->time_total += time_value;
    list->length += 1;
}

const char zero[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfc, 0xfb, 0xf7, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xf7, 0xfb, 0xfc, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xfe, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0xdf, 0xef, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xef, 0xdf, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const char one[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf8,
    0xfc, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x7f,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfe,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x1f,
    0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const char two[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xf7, 0xfb, 0xfc, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xbf, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xfe, 0xfd, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0xdf, 0xef, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xe0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const char three[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xf7, 0xfb, 0xfc, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xbf, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0xfd, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xef, 0xdf, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const char four[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfc, 0xf8, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf8, 0xfc, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f, 0xbf, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xbf, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0xfd, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x1f, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const char five[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfc, 0xfb, 0xf7, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f, 0xbf, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0xfd, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xef, 0xdf, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const char six[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfc, 0xfb, 0xf7, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f, 0xbf, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xfe, 0xfd, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0xfd, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0xdf, 0xef, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xef, 0xdf, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const char seven[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xf8, 0xf6, 0xee, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0x1e, 0xee, 0xf6, 0xf8, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xfc, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x3f, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

};

const char eight[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfc, 0xfb, 0xf7, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xf7, 0xfb, 0xfc, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f, 0xbf, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xbf, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xfe, 0xfd, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0xfd, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0xdf, 0xef, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xef, 0xdf, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const char nine[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xfc, 0xfb, 0xf7, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xf7, 0xfb, 0xfc, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x7f, 0xbf, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xbf, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0xfd, 0xfe, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
    0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xef, 0xdf, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const char top_frame[] PROGMEM = {
    0x10, 0x18, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c,
    0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x18, 0x10
};

const char bottom_frame[] PROGMEM = {
    0x08, 0x18, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
    0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x18, 0x08
};

const char* const numbers[] PROGMEM = { zero, one, two, three, four, five, six, seven, eight, nine };

// http://www.keyboard-layout-editor.com/##@_name=crkbd&author=taini%3B&@_x:3%3B&=%23%0A3%0A%0A%0A%0A%0AE&_x:7%3B&=*%0A8%0A%0A%0A%0A%0AI%3B&@_y:-0.9&x:2%3B&=%2F@%0A2%0A%0A%0A%0A%0AW%3B&@_y:-1&x:4%3B&=$%0A4%0A%0A%0A%0A%0AR&_x:5%3B&=%2F&%0A7%0A%0A%0A%0A%0AU&_x:1%3B&=(%0A9%0A%0A%0A%0A%0AO%3B&@_y:-0.9&x:5%3B&=%25%0A5%0A%0A%0A%0A%0AT&_x:3%3B&=%5E%0A6%0A%0A%0A%0A%0AY%3B&@_y:-0.9&a:0%3B&=Esc%0AEsc%0A%0A%0ARST%0A%0ATab&_a:4%3B&=!%0A1%0A%0A%0A%0A%0AQ&_x:11%3B&=)%0A0%0A%0A%0A%0A%0AP&_a:6%3B&=Bksp%3B&@_y:-0.30000000000000004&x:3&a:4%3B&=F4%0A%3Ci%20class%2F='kb%20kb-Multimedia-Rewind-Start'%3E%3C%2F%2Fi%3E%0A%0A%0A%0A%0AD&_x:7%3B&=%7B%0A%3Ci%20class%2F='kb%20kb-Arrows-Up'%3E%3C%2F%2Fi%3E%0A%0A%0A%0A%0AK%3B&@_y:-0.8999999999999999&x:2%3B&=F3%0A%3Ci%20class%2F='kb%20kb-Multimedia-Stop'%3E%3C%2F%2Fi%3E%0A%0A%0A%0A%0AS&_x:1%3B&=F5%0A%3Ci%20class%2F='kb%20kb-Multimedia-FastForward-End'%3E%3C%2F%2Fi%3E%0A%0A%0A%0A%0AF&_x:5%3B&=%2F=%0A%3Ci%20class%2F='kb%20kb-Arrows-Down'%3E%3C%2F%2Fi%3E%0A%0A%0A%0A%0AJ&_x:1%3B&=%7D%0A%3Ci%20class%2F='kb%20kb-Arrows-Right'%3E%3C%2F%2Fi%3E%0A%0A%0A%0A%0AL%3B&@_y:-0.8999999999999999&x:5%3B&=F6%0A%0A%0A%0A%0A%0AG&_x:3%3B&=-%0A%3Ci%20class%2F='kb%20kb-Arrows-Left'%3E%3C%2F%2Fi%3E%0A%0A%0A%0A%0AH%3B&@_y:-0.8999999999999999%3B&=F1%0A%0A%0A%0A%0A%0ACtrl&=F2%0A%3Ci%20class%2F='kb%20kb-Multimedia-Play-Pause'%3E%3C%2F%2Fi%3E%0A%0A%0A%0A%0AA&_x:11%3B&=%7C%0A%0A%0A%0A%0A%0A%2F:&=%60%0ADel%0A%0A%0A%0A%0A%22%3B&@_y:-0.2999999999999998&x:3%3B&=F10%0A%0A%0A%0A%0A%0AC&_x:7%3B&=%5B%0APgUp%0A%0A%0A%0A%0A.%3B&@_y:-0.8999999999999999&x:2%3B&=F9%0A%0A%0A%0A%0A%0AX&_x:1%3B&=F11%0A%0A%0A%0A%0A%0AV&_x:5%3B&=+%0APgDn%0A%0A%0A%0A%0AM&_x:1%3B&=%5D%0AEnd%0A%0A%0A%0A%0A,%3B&@_y:-0.8999999999999999&x:5%3B&=F12%0A%0A%0A%0A%0A%0AB&_x:3%3B&=%2F_%0AHome%0A%0A%0A%0A%0AN%3B&@_y:-0.8999999999999999%3B&=F7%0A%0A%0A%0A%0A%0AShift&=F8%0A%0A%0A%0A%0A%0AZ&_x:11%3B&=%5C%0A%0A%0A%0A%0A%0A%2F%2F&=~%0AIns%0A%0A%0A%0A%0AR%2F_Alt%3B&@_y:-0.20000000000000018&x:3.5&a:6%3B&=L%2F_Alt&_x:6%3B&=Super%3B&@_r:15&rx:4.5&ry:4.1&y:-1%3B&=Lower%3B&@_r:30&rx:5.4&ry:4.3&y:-1.5&x:0.09999999999999964&h:1.5%3B&=SPC%3B&@_r:-30&rx:9.6&y:-1.5&x:-1.0999999999999996&h:1.5%3B&=Enter%3B&@_r:-15&rx:10.5&ry:4.1&y:-1&x:-1%3B&=Raise

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [_QWERTY] = LAYOUT( \
  //,-----------------------------------------.                ,-----------------------------------------.
        KC_TAB,     KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,                      KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,  KC_BSPC,\
  //|------+------+------+------+------+------|                |------+------+------+------+------+------|
       KC_LCTL,     KC_A,     KC_S,     KC_D,     KC_F,     KC_G,                      KC_H,     KC_J,     KC_K,     KC_L,  KC_SCLN,  KC_QUOT,\
  //|------+------+------+------+------+------|                |------+------+------+------+------+------|
       KC_LSFT,     KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,                      KC_N,     KC_M,  KC_COMM,   KC_DOT,  KC_SLSH,  KC_RSFT,\
  //|------+------+------+------+------+------+------|  |------+------+------+------+------+------+------|
                                   KC_LALT  , KC_LOWER,   KC_SPC, KC_ENT, KC_RAISE,  KC_LGUI       \
                              //`--------------------'  `--------------------'
  ) ,

  [_LOWER] = LAYOUT( \
  //,-----------------------------------------.                ,-----------------------------------------.
        KC_ESC,  KC_EXLM,    KC_AT,  KC_HASH,   KC_DLR,  KC_PERC,                   KC_CIRC,  KC_AMPR,  KC_ASTR,  KC_LPRN,  KC_RPRN, KC______,\
  //|------+------+------+------+------+------|                |------+------+------+------+------+------|
        KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,                   KC_MINS,   KC_EQL,  KC_LBRC,  KC_RBRC,  KC_PIPE,   KC_GRV,\
  //|------+------+------+------+------+------|                |------+------+------+------+------+------|
        KC_F7,    KC_F8,    KC_F9,   KC_F10,   KC_F11,   KC_F12,                   KC_UNDS,  KC_PLUS,  KC_LCBR,  KC_RCBR,  KC_BSLS,  KC_TILD,\
  //|------+------+------+------+------+------+------|  |------+------+------+------+------+------+------|
                                  KC_RALT  , KC______, KC______,    KC______, KC______, KC______ \
                              //`--------------------'  `--------------------'
  ),

  [_RAISE] = LAYOUT( \
  //,-----------------------------------------.                ,-----------------------------------------.
        KC_ESC,     KC_1,     KC_2,     KC_3,     KC_4,     KC_5,                      KC_6,     KC_7,     KC_8,     KC_9,     KC_0, KC______,\
  //|------+------+------+------+------+------|                |------+------+------+------+------+------|
      KC______,  KC_MPLY,  KC_MSTP,  KC_MPRV,  KC_MNXT, KC______,                   KC_LEFT,  KC_DOWN,    KC_UP, KC_RIGHT, KC______,   KC_DEL,\
  //|------+------+------+------+------+------|                |------+------+------+------+------+------|
      KC______, KC______, KC______, KC______, KC______, KC______,                   KC_HOME,  KC_PGDN,  KC_PGUP,   KC_END, KC______,   KC_INS,\
  //|------+------+------+------+------+------+------|  |------+------+------+------+------+------+------|
                                  KC_RALT, KC______, KC______,    KC______, KC______, KC______ \
                              //`--------------------'  `--------------------'
  ),

  [_ADJUST] = LAYOUT( \
  //,-----------------------------------------.                ,-----------------------------------------.
        KC_RST,  KC______, KC______, KC______, KC______, KC______,                  KC______, KC______, KC______, KC______, KC______, KC______,\
  //|------+------+------+------+------+------|                |------+------+------+------+------+------|
      KC______,  KC______, KC______, KC______, KC______, KC______,                  KC______, KC______, KC______, KC______, KC______, KC______,\
  //|------+------+------+------+------+------|                |------+------+------+------+------+------|
      KC______,  KC______, KC______, KC______, KC______, KC______,                  KC______, KC______, KC______, KC______, KC______, KC______,\
  //|------+------+------+------+------+------+------|  |------+------+------+------+------+------+------|
                                  KC______, KC______, KC______,    KC______, KC______, KC______ \
                              //`--------------------'  `--------------------'
  )
};

// Setting ADJUST layer RGB back to default
void update_tri_layer_RGB(uint8_t layer1, uint8_t layer2, uint8_t layer3) {
  if (IS_LAYER_ON(layer1) && IS_LAYER_ON(layer2)) {
    layer_on(layer3);
  } else {
    layer_off(layer3);
  }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        oled_timer = timer_read32();
      // if (record->event.key.row <= 3) {
        if (is_master && record->event.key.row <= 3) {
            add_time_node(&master_list, &kpm_timer_master);
        } else if (!is_master && record->event.key.row > 3)  {
            add_time_node(&slave_list, &kpm_timer_slave);
        }
        // uprintf("%d - %d\n", master_list.length, slave_list.length);
    }

    switch (keycode) {
        case QWERTY:
            if (record->event.pressed) {
                set_single_persistent_default_layer(_QWERTY);
            }
            return false;
            break;
        case LOWER:
            if (record->event.pressed) {
                layer_on(_LOWER);
                update_tri_layer_RGB(_LOWER, _RAISE, _ADJUST);
            } else {
                layer_off(_LOWER);
                update_tri_layer_RGB(_LOWER, _RAISE, _ADJUST);
            }
            return false;
            break;
        case RAISE:
            if (record->event.pressed) {
                layer_on(_RAISE);
                update_tri_layer_RGB(_LOWER, _RAISE, _ADJUST);
            } else {
                layer_off(_RAISE);
                update_tri_layer_RGB(_LOWER, _RAISE, _ADJUST);
            }
            return false;
            break;
        case ADJUST:
            if (record->event.pressed) {
                layer_on(_ADJUST);
            } else {
                layer_off(_ADJUST);
            }
            return false;
            break;
    }
    return true;
}


oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return OLED_ROTATION_270;
}

void render_kpm(time_list* list, uint32_t* kpm_timer) {
  check_time_list(list, kpm_timer);
    char digits[512];
    memset(digits, 0, sizeof(digits));

    if (list->length == 0) {
      oled_write_raw(digits, sizeof(digits));
      return;
    }

    int first_number  = (int)(list->length / pow(10, 2 )) % 10;
    int second_number = (int)(list->length / pow(10, 1)) % 10;
    int number_length = floor(log10(abs(list->length)));

    for (int i = 0; i < 3; i++) {
      if ((i == 0 && first_number == 0) || (i ==1 && second_number == 0 && first_number == 0)) {
        continue;
      }
      int tmp = list->length / pow(10, 2 - i);
      int number = tmp % 10;

      int offset = DIGIT_OFFSET * (5 - 2 * number_length) + ((i - (2 - number_length)) * (DIGIT_SIZE + DIGIT_OFFSET));
      memcpy_P(digits + offset, (char *)pgm_read_word(&(numbers[number])), DIGIT_SIZE);
    }

    // render frame
    if (timer_elapsed(kpm_timer_frame) > FRAME_UPDATE) {
        kpm_timer_frame = timer_read();
        int direction = frame_value * 200 > list->time_total ? -1 : 1;
        direction = abs(frame_value * 200 - list->time_total) < 200 ? 0 : direction;
        frame_value += direction;
        frame_value = frame_value > 301 ? 301 : frame_value;
        frame_value = frame_value < 0 ? 0 : frame_value;
    }
    // render top line
    memcpy_P(digits, top_frame, frame_value < sizeof(top_frame) ? frame_value : sizeof(top_frame));
    // render top right corner
    if (frame_value >= 32) {
        int offset = frame_value > 34 ? 2 : frame_value - 32;
        int progress[] = {32, 96, 224};
        digits[29] |= progress[offset];
        digits[30] |= progress[offset];
        digits[31] |= progress[offset];
    }
    // render right line
    for (int i = 35; i <= frame_value && i <= 147; i += 8) {
        int row = (int)(i / 8) - 2;
        int progress = frame_value - i > 8 ? 8 : frame_value - i;
        digits[(row * 32) - 1] |= (int)(pow(2, progress) + 0.5) - 1;
        digits[(row * 32) - 2] |= (int)(pow(2, progress) + 0.5) - 1;
        digits[(row * 32) - 3] |= (int)(pow(2, progress) + 0.5) - 1;
    }
    // render bottom right corner
    if (frame_value >= 148) {
        int offset = frame_value > 150 ? 2 : frame_value - 148;
        int progress[] = {1, 3, 7};
        digits[509] = progress[offset];
        digits[510] = progress[offset];
        digits[511] = progress[offset];
    }
    // render bottom line
    for (int i = 151; i <= frame_value && i <= 182; i++) {
        int position = 182 - i;
        digits[480 + position] |= pgm_read_byte(bottom_frame + position);
    }
    // render bottom left corner
    if (frame_value >= 183) {
        int offset = frame_value > 185 ? 2 : frame_value - 183;
        int progress[] = {4, 6, 7};
        digits[480] |= progress[offset];
        digits[481] |= progress[offset];
        digits[482] |= progress[offset];
    }
    // render left line
    for (int i = 186; i <= frame_value && i <= 298; i += 8) {
        int row = (int)((298 - i) / 8);
        int progress = frame_value - i > 8 ? 8 : frame_value - i;
        digits[(row * 32) + 0] |= (uint8_t)(65280 >> progress);
        digits[(row * 32) + 1] |= (uint8_t)(65280 >> progress);
        digits[(row * 32) + 2] |= (uint8_t)(65280 >> progress);
    }
    // render top left corner
    if (frame_value >= 298) {
        digits[0] |= 224;
        digits[1] |= 224;
        digits[2] |= 224;
    }


    oled_write_raw(digits, sizeof(digits));
}

void oled_task_user(void) {
    if (is_master) {
      render_kpm(&master_list, &kpm_timer_master);
    } else {
      render_kpm(&slave_list, &kpm_timer_slave);
    }
}
