#include <pebble.h>
#include "big_util.h"
static Window *s_main_window;
// There's only enough memory to load about 6 of 10 required images
// so we have to swap them in & out...
//
// We have one "slot" per digit location on screen.
//
// Because layers can only have one parent we load a digit for each
// slot--even if the digit image is already in another slot.
//
// Slot on-screen layout:
// 0 1
// 2 

// Original Code

#define TOTAL_IMAGE_SLOTS 4
#define NUMBER_OF_IMAGES 10
#define BARLENGTH 25
#define BARWIDTH 6
#define BARMARGIN 3
#define MARGIN 14

bool change_state = false;
static InverterLayer *s1_bar, *s2_bar, *s3_bar, *s4_bar, *s5_bar;
static GBitmap *s_images[TOTAL_IMAGE_SLOTS];
static BitmapLayer *s_image_layers[TOTAL_IMAGE_SLOTS];
#define EMPTY_SLOT -1
  
// Settings - New Code
#define USE_AMERICAN_DATE_FORMAT      false

// Magic numbers
#define SCREEN_WIDTH        144
#define SCREEN_HEIGHT       168


#define DATE_IMAGE_WIDTH    28
#define DATE_IMAGE_HEIGHT   28

#define DAY_IMAGE_WIDTH     20
#define DAY_IMAGE_HEIGHT    10

#define TIME_SLOT_SPACE     2
#define DATE_PART_SPACE     4

// These images are 72 x 84 pixels (i.e. a quarter of the display),
// black and white with the digit character centered in the image.
// (As generated by the `fonttools/font2png.py` script.)
const int IMAGE_RESOURCE_IDS[NUMBER_OF_IMAGES] = {
    RESOURCE_ID_NUM_0, RESOURCE_ID_NUM_1, RESOURCE_ID_NUM_2,
    RESOURCE_ID_NUM_3, RESOURCE_ID_NUM_4, RESOURCE_ID_NUM_5,
    RESOURCE_ID_NUM_6, RESOURCE_ID_NUM_7, RESOURCE_ID_NUM_8,
    RESOURCE_ID_NUM_9
};

//Date Images
#define NUMBER_OF_DATE_IMAGES 10
const int DATE_IMAGE_RESOURCE_IDS[NUMBER_OF_DATE_IMAGES] = {
  RESOURCE_ID_DATE_0, 
  RESOURCE_ID_DATE_1, RESOURCE_ID_DATE_2, RESOURCE_ID_DATE_3, 
  RESOURCE_ID_DATE_4, RESOURCE_ID_DATE_5, RESOURCE_ID_DATE_6, 
  RESOURCE_ID_DATE_7, RESOURCE_ID_DATE_8, RESOURCE_ID_DATE_9
};


// General
static Window *window;


#define EMPTY_SLOT -1
typedef struct Slot {
  int         number;
  GBitmap     *image;
  BitmapLayer *image_layer;
  int         state;
} Slot;


// Footer
static Layer *footer_layer;

// Date
#define NUMBER_OF_DATE_SLOTS 4
static Layer *date_layer;
static Slot date_slots[NUMBER_OF_DATE_SLOTS];

// General
BitmapLayer *load_date_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids);
void unload_date_image_from_slot(Slot *slot);

// Date
void display_date(struct tm *tick_time);
void display_date_value(int value, int part_number);
void update_date_slot(Slot *date_slot, int digit_value);


// The state is either "empty" or the digit of the image currently in
// the slot--which was going to be used to assist with de-duplication
// but we're not doing that due to the one parent-per-layer
// restriction mentioned above.
static int s_image_slot_state[TOTAL_IMAGE_SLOTS] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};
static void load_digit_image_into_slot(int slot_number, int digit_value) {
    /*
    * Loads the digit image from the application's resources and
    * displays it on-screen in the correct location.
    *
    * Each slot is a quarter of the screen.
    */
    if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
        return;
    }
    if ((digit_value < 0) || (digit_value > 9)) {
        return;
    }
    if (s_image_slot_state[slot_number] != EMPTY_SLOT) {
        return;
    }
    s_image_slot_state[slot_number] = digit_value;
    s_images[slot_number] = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS[digit_value]);
    #ifdef PBL_PLATFORM_BASALT
    GRect bounds = gbitmap_get_bounds(s_images[slot_number]);
    #else
    GRect bounds = s_images[slot_number]->bounds;
    #endif
    BitmapLayer *bitmap_layer = bitmap_layer_create(GRect((((slot_number % 2) * 62)+12), ((slot_number / 2) * 62)+8, bounds.size.w, bounds.size.h));
    s_image_layers[slot_number] = bitmap_layer;
    bitmap_layer_set_bitmap(bitmap_layer, s_images[slot_number]);
    Layer *window_layer = window_get_root_layer(s_main_window);
    layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
}

static void unload_digit_image_from_slot(int slot_number) {
    if (s_image_slot_state[slot_number] != EMPTY_SLOT) {
        layer_remove_from_parent(bitmap_layer_get_layer(s_image_layers[slot_number]));
        bitmap_layer_destroy(s_image_layers[slot_number]);
        gbitmap_destroy(s_images[slot_number]);
        s_image_slot_state[slot_number] = EMPTY_SLOT;
    }
}


// General

BitmapLayer *load_date_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids) {
  if (digit_value < 0 || digit_value > 9)
    return NULL;

  if (slot->state != EMPTY_SLOT)
    return NULL;

  slot->state = digit_value;

  slot->image = gbitmap_create_with_resource(digit_resource_ids[digit_value]);

  slot->image_layer = bitmap_layer_create(frame);
  bitmap_layer_set_bitmap(slot->image_layer, slot->image);
  layer_add_child(parent_layer, bitmap_layer_get_layer(slot->image_layer));

  return slot->image_layer;
}

//General Date

void unload_date_image_from_slot(Slot *slot) {
  if (slot->state == EMPTY_SLOT)
    return;

  layer_remove_from_parent(bitmap_layer_get_layer(slot->image_layer));
  bitmap_layer_destroy(slot->image_layer);

  gbitmap_destroy(slot->image);

  slot->state = EMPTY_SLOT;
}
static void display_value(unsigned short value, unsigned short row_number, bool show_first_leading_zero) {
    value = value % 100; // Maximum of two digits per row.
    // Column order is: | Column 0 | Column 1 |
    // (We process the columns in reverse order because that makes
    // extracting the digits from the value easier.)
    for (int column_number = 1; column_number >= 0; column_number--) {
        int slot_number = (row_number * 2) + column_number;
        unload_digit_image_from_slot(slot_number);
        load_digit_image_into_slot(slot_number, value % 10);
        
        value = value / 10;
    }
}
// Date
void display_date(struct tm *tick_time) {
  int day   = tick_time->tm_mday;
  int month = tick_time->tm_mon + 1;

#if USE_AMERICAN_DATE_FORMAT
  display_date_value(month, 0);
  display_date_value(day,   1);
#else
  display_date_value(day,   0);
  display_date_value(month, 1);
#endif
}

void update_date_slot(Slot *date_slot, int digit_value) {
  if (date_slot->state == digit_value)
    return;

  int x = date_slot->number * (DATE_IMAGE_WIDTH + MARGIN);
  if (date_slot->number >= 2) {
    x += 3; // 3 extra pixels of space between the day and month
  }
  GRect frame =  GRect(x, 0, DATE_IMAGE_WIDTH, DATE_IMAGE_HEIGHT);

  unload_date_image_from_slot(date_slot);
  load_date_image_into_slot(date_slot, digit_value, date_layer, frame, DATE_IMAGE_RESOURCE_IDS);
}

void display_date_value(int value, int part_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {
    int date_slot_number = (part_number * 2) + column_number;

    Slot *date_slot = &date_slots[date_slot_number];

    update_date_slot(date_slot, value % 10);

    value = value / 10;
  }
}

static unsigned short get_display_hour(unsigned short hour) {
    if (clock_is_24h_style()) {
        return hour;
    }
    unsigned short display_hour = hour % 12;
    // Converts "0" to "12"
    return display_hour ? display_hour : 12;
}
static void display_time(struct tm *tick_time) {
    display_value(get_display_hour(tick_time->tm_hour), 0, false);
    display_value(tick_time->tm_min, 1, true);
}
static void handle_tick(struct tm *t, TimeUnits units_changed)
{  
    //Get the time
    int seconds = t->tm_sec;
    //Bottom suface
    switch(seconds)
    {
        case 0:
        display_time(t);
        change_state = false;
        break;
        case 10:
        cl_animate_layer(inverter_layer_get_layer(s1_bar), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0);
        break;
        case 20:
        cl_animate_layer(inverter_layer_get_layer(s2_bar),  GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN + 1, 0, BARLENGTH), GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0);
        break;
        case 30:
        cl_animate_layer(inverter_layer_get_layer(s3_bar),  GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN + 1, 0, BARLENGTH), GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0);
        break;
        case 40:
        cl_animate_layer(inverter_layer_get_layer(s4_bar),  GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN + 1, 0, BARLENGTH), GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0);
        break;
        case 50:
        cl_animate_layer(inverter_layer_get_layer(s5_bar),  GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0);
        break;
    }
    if(seconds >= 58 && change_state == false){
        int timeleft = (60 - seconds)*150;
         change_state = true;
        cl_animate_layer(inverter_layer_get_layer(s1_bar), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, 0, BARLENGTH), 500, 5*timeleft);
        cl_animate_layer(inverter_layer_get_layer(s2_bar),  GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN + 1, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN, 0, BARLENGTH), 500, 4*timeleft);
        cl_animate_layer(inverter_layer_get_layer(s3_bar),  GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN + 1, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN, 0, BARLENGTH), 500, 3*timeleft);
        cl_animate_layer(inverter_layer_get_layer(s4_bar),  GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN + 1, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN, 0, BARLENGTH), 500, 2*timeleft);
        cl_animate_layer(inverter_layer_get_layer(s5_bar),  GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, 0, BARLENGTH), 500, timeleft);
      // 140 Pixels of line
        
    }
    if ((units_changed & DAY_UNIT) == DAY_UNIT) {
    display_date(t);
  }
    
}
static void main_window_load(Window *window) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    display_time(t);
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
    s1_bar = inverter_layer_create(GRect(0, 8, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(s1_bar));
    s2_bar = inverter_layer_create(GRect(0, 30, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(s2_bar));
    s3_bar = inverter_layer_create(GRect(0, 52, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(s3_bar));
    s4_bar = inverter_layer_create(GRect(0, 74, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(s4_bar));
    s5_bar = inverter_layer_create(GRect(0, 96, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(s5_bar));
    //Init seconds bar
    int seconds = t->tm_sec;
        if (seconds > 10){
         cl_animate_layer(inverter_layer_get_layer(s1_bar), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0); 
        }
        if (seconds > 20){
         cl_animate_layer(inverter_layer_get_layer(s2_bar),  GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN + 1, 0, BARLENGTH), GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN, BARWIDTH, BARLENGTH), 500, 100); 
        }  
        if (seconds > 30){
         cl_animate_layer(inverter_layer_get_layer(s3_bar),  GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN + 1, 0, BARLENGTH), GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN, BARWIDTH, BARLENGTH), 500, 200); 
        }        

        if (seconds > 40){
         cl_animate_layer(inverter_layer_get_layer(s4_bar),  GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN + 1, 0, BARLENGTH), GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN, BARWIDTH, BARLENGTH), 500, 300); 
        }        
        if (seconds > 50){
         cl_animate_layer(inverter_layer_get_layer(s5_bar),  GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, BARWIDTH, BARLENGTH), 500, 400); 
        }
                      
  
}
static void main_window_unload(Window *window) {
    for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
        unload_digit_image_from_slot(i);
    }
          	inverter_layer_destroy(s1_bar);
        inverter_layer_destroy(s2_bar);
        inverter_layer_destroy(s3_bar);
        inverter_layer_destroy(s4_bar);
        inverter_layer_destroy(s5_bar);
}
static void init() {
    s_main_window = window_create();
    window_set_background_color(s_main_window, GColorBlack);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });
    window_stack_push(s_main_window, true);
    window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  Layer *root_layer = window_get_root_layer(window);

  // Footer
  int footer_height = SCREEN_HEIGHT - SCREEN_WIDTH + 28;

  footer_layer = layer_create(GRect(0, SCREEN_WIDTH, SCREEN_WIDTH, footer_height));
  layer_add_child(root_layer, footer_layer);
  // Date
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    Slot *date_slot = &date_slots[i];
    date_slot->number = i;
    date_slot->state  = EMPTY_SLOT;
  }

  GRect date_layer_frame = GRectZero;
  date_layer_frame.size.w   = DATE_IMAGE_WIDTH + 2 + DATE_IMAGE_WIDTH + 4 + DATE_IMAGE_WIDTH + 2 + DATE_IMAGE_WIDTH;
  date_layer_frame.size.h   = DATE_IMAGE_HEIGHT;
  date_layer_frame.origin.x = 14;
  date_layer_frame.origin.y = 140;

  date_layer = layer_create(date_layer_frame);
  layer_add_child(footer_layer, date_layer);



  // Display
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  display_date(tick_time);
}
static void deinit() {
    window_destroy(s_main_window);
    // Date
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    unload_date_image_from_slot(&date_slots[i]);
  }
  layer_destroy(date_layer);

  // Seconds

  layer_destroy(footer_layer);

  window_destroy(window);
}
int main(void) {
    init();
    app_event_loop();
    deinit();
}