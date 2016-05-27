#include <pebble.h>
#include "util.h"
#include "InverterLayerCompat.h"
static Window *s_main_window;

// Number of images for each array
#define TOTAL_IMAGE_SLOTS 4
#define NUMBER_OF_IMAGES 10
#define NUMBER_OF_DAY_IMAGES 7
#define NUMBER_OF_DATE_IMAGES 10

// Seconds Bar Settings
#define BARLENGTH 25
#define BARWIDTH 6
#define BARMARGIN 3
#define MARGIN 14

// Changes it to a Monday version
#define MonStart  false

// Arrays for all the images in the project

// Date Bar on right side - tranisitioning to draw elements
// 6 x 18 pixel bars
// 14 pixels from  top
// 2 pixel gap
const int DAY_IMAGE_RESOURCE_IDS[NUMBER_OF_DAY_IMAGES] = {
  RESOURCE_ID_DAY_1, RESOURCE_ID_DAY_2, 
  RESOURCE_ID_DAY_3, RESOURCE_ID_DAY_4, RESOURCE_ID_DAY_5, 
  RESOURCE_ID_DAY_6, RESOURCE_ID_DAY_7
};

// Main Time Elements( Top 4/5 of the watch)
// These images are 60 x 60 pixels
// Should start 8 pixels from top, 12 pixels from the left
// 2 pixel gap between numbers
const int IMAGE_RESOURCE_IDS[NUMBER_OF_IMAGES] = {
    RESOURCE_ID_NUM_0, RESOURCE_ID_NUM_1, RESOURCE_ID_NUM_2,
    RESOURCE_ID_NUM_3, RESOURCE_ID_NUM_4, RESOURCE_ID_NUM_5,
    RESOURCE_ID_NUM_6, RESOURCE_ID_NUM_7, RESOURCE_ID_NUM_8,
    RESOURCE_ID_NUM_9
};

// Date Elements( Bottom 1/5 of the Watch)
// These Images are 28 x 28 pixels
// Same width as main time elements
// Should start 134 pixels from the top(4 pixels down from the main time elements) 12 pixels from the left
// 3 pixel between double digits - 4 pixels between day/month  
const int DATE_IMAGE_RESOURCE_IDS[NUMBER_OF_DATE_IMAGES] = {
  RESOURCE_ID_DATE_0, 
  RESOURCE_ID_DATE_1, RESOURCE_ID_DATE_2, RESOURCE_ID_DATE_3, 
  RESOURCE_ID_DATE_4, RESOURCE_ID_DATE_5, RESOURCE_ID_DATE_6, 
  RESOURCE_ID_DATE_7, RESOURCE_ID_DATE_8, RESOURCE_ID_DATE_9
};

// Second layers

// Inversion libary
#ifdef PBL_SDK_2
static InverterLayer *s1_bar, *s2_bar, *s3_bar, *s4_bar, *s5_bar;
#elif PBL_SDK_3
static InverterLayerCompat *s1_bar, *s2_bar, *s3_bar, *s4_bar, *s5_bar;
#endif

static GColor fg_color, bg_color;

// Used to stop multiple animations at once
bool change_state = false;

// Slots for main time element
static GBitmap *s_images[TOTAL_IMAGE_SLOTS];
static BitmapLayer *s_image_layers[TOTAL_IMAGE_SLOTS];

// Slots for the date element
static GBitmap *d_images[4];
static BitmapLayer *d_image_layers[4];

// Slot for the week day element
static GBitmap *wd_image;
static BitmapLayer *wd_image_layer;
#define EMPTY_SLOT -1

// Tracks the state of the slots
static int s_image_slot_state[TOTAL_IMAGE_SLOTS] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};
static int d_image_slot_state[4] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};

static void load_digit_image_into_slot(int slot_number, int digit_value) {
// Validation of the paramaters
// Makes sure the slot number is valid, the number is in range, and that the slot is empty
    if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
        return;
    }
    if ((digit_value < 0) || (digit_value > 9)) {
        return;
    }
    if (s_image_slot_state[slot_number] != EMPTY_SLOT) {
        return;
    }
  
// Sets the status of the slot
    s_image_slot_state[slot_number] = digit_value;
// Loads it in
    s_images[slot_number] = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS[digit_value]);
    GRect bounds = gbitmap_get_bounds(s_images[slot_number]);
// Math for positioning the bitmap
    BitmapLayer *bitmap_layer = bitmap_layer_create(GRect((((slot_number % 2) * 62)+12), ((slot_number / 2) * 62)+8, bounds.size.w, bounds.size.h));
// Moves layer into array
    s_image_layers[slot_number] = bitmap_layer; 
// Moves image into layer
    bitmap_layer_set_bitmap(bitmap_layer, s_images[slot_number]);
// Makes it a child of the main layer.
    Layer *window_layer = window_get_root_layer(s_main_window);
    layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
}

static void unload_digit_image_from_slot(int slot_number) {
 // Validation that the slot isn't already empty
    if (s_image_slot_state[slot_number] != EMPTY_SLOT) {
        // Removes layer from the window
        layer_remove_from_parent(bitmap_layer_get_layer(s_image_layers[slot_number]));
        // Loads it out of memory
        bitmap_layer_destroy(s_image_layers[slot_number]);
        gbitmap_destroy(s_images[slot_number]);
        // Assigns as empty again
        s_image_slot_state[slot_number] = EMPTY_SLOT;
    }
}

// Takes the time and loads it in.
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

static void load_date_image_into_slot(int slot_number, int digit_value) {
  // Used to create extra spacing
    int margin_extra = 0;
  // Validation - same as main time eleemnt
    if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
        return; 
    }
    if ((digit_value < 0) || (digit_value > 9)) {
        return;
            
    }
   if (d_image_slot_state[slot_number] != EMPTY_SLOT) {
        return;
   }
  // Checking if it is the day or month and adjusts the margins as such
    if(slot_number > 1){
      margin_extra = 1;
    }  
    else{
      margin_extra = 0;    
    }
  // Updating status 
    d_image_slot_state[slot_number] = digit_value;
    d_images[slot_number] = gbitmap_create_with_resource(DATE_IMAGE_RESOURCE_IDS[digit_value]);
  // Math for positioning
    BitmapLayer *bitmap_layer = bitmap_layer_create(GRect((((slot_number) * 31)+12+margin_extra), 134, 28, 28));
  // Creates image
    d_image_layers[slot_number] = bitmap_layer;
    bitmap_layer_set_bitmap(bitmap_layer, d_images[slot_number]);
    Layer *window_layer = window_get_root_layer(s_main_window);
    layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
}

static void unload_date_image_from_slot(int slot_number) {
   // Validation that the slot isn't already empty
    if (d_image_slot_state[slot_number] != EMPTY_SLOT) {
      // Unloading from memory
        layer_remove_from_parent(bitmap_layer_get_layer(d_image_layers[slot_number]));
        bitmap_layer_destroy(d_image_layers[slot_number]);
        gbitmap_destroy(d_images[slot_number]);
      //Update Status
        d_image_slot_state[slot_number] = EMPTY_SLOT;
    }
}

// Takes the date and loads it in  
static void display_value_date(unsigned short value, unsigned short row_number, bool show_first_leading_zero) {
    value = value % 100; // Maximum of two digits per row
    // Column order is: | Column 0 | Column 1 |
    // (We process the columns in reverse order because that makes
    // extracting the digits from the value easier.)

    for (int column_number = 1; column_number >= 0; column_number--) {
        int slot_number = (row_number * 2) + column_number;
        unload_date_image_from_slot(slot_number);
        load_date_image_into_slot(slot_number, value % 10);
        value = value / 10;
      
    }
}

// Display syle conversion
static unsigned short get_display_hour(unsigned short hour) {
    if (clock_is_24h_style()) {
        return hour;
    }
    unsigned short display_hour = hour % 12;
    // Converts "0" to "12"
    return display_hour ? display_hour : 12;
}
// Displaying the time
static void display_time(struct tm *tick_time) {
    display_value(get_display_hour(tick_time->tm_hour), 0, false);
    display_value(tick_time->tm_min, 1, true);
}
// Displaying the Date
static void display_month(struct tm *tick_time){
  display_value_date(tick_time->tm_mday,0,true);
  display_value_date((tick_time->tm_mon)+1,1,true);
}
void unload_day_item() {

        layer_remove_from_parent(bitmap_layer_get_layer(wd_image_layer));
        bitmap_layer_destroy(wd_image_layer);
        gbitmap_destroy(wd_image);
}

void display_day(struct tm *tick_time) {
  unload_day_item();
#if MonStart
int WkDay = tick_time->tm_wday - 1;
  if (WkDay < 0){
    WkDay = 6;
  }
#else
 int WkDay = tick_time->tm_wday;
#endif
      
    wd_image = gbitmap_create_with_resource(DAY_IMAGE_RESOURCE_IDS[WkDay]);
    GRect bounds = gbitmap_get_bounds(wd_image);

    BitmapLayer *bitmap_layer = bitmap_layer_create(GRect(138,MARGIN, bounds.size.w, bounds.size.h));
    wd_image_layer = bitmap_layer;
    bitmap_layer_set_bitmap(bitmap_layer, wd_image);
    Layer *window_layer = window_get_root_layer(s_main_window);
    layer_add_child(window_layer, bitmap_layer_get_layer(bitmap_layer));
}
 	
void bt_handler(bool connected) {
            if (connected == true) {
                vibes_double_pulse();       
              } else {
                vibes_long_pulse();
            }
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
        util_animate_layer(inverter_layer_compat_func_get(s1_bar), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0);
        break;
        case 20:
        util_animate_layer(inverter_layer_compat_func_get(s2_bar), GRect(0, MARGIN + 1*BARLENGTH + 1*1, 0, BARLENGTH), GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN, BARWIDTH, BARLENGTH),  500, 0);
        break;
        case 30:
        util_animate_layer(inverter_layer_compat_func_get(s3_bar), GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0);
        break;
        case 40:
        util_animate_layer(inverter_layer_compat_func_get(s4_bar),  GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN, BARWIDTH, BARLENGTH),  500, 0);
        break;
        case 50:
        util_animate_layer(inverter_layer_compat_func_get(s5_bar),  GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0);
        break;
    }
    if(seconds >= 58 && change_state == false){
         int timeleft = (60 - seconds)*150;
         change_state = true;
       util_animate_layer(inverter_layer_compat_func_get(s1_bar), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, 0, BARLENGTH), 500, 5*timeleft);
       util_animate_layer(inverter_layer_compat_func_get(s2_bar),  GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN, 0, BARLENGTH), 500, 4*timeleft);
       util_animate_layer(inverter_layer_compat_func_get(s3_bar),  GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN, 0, BARLENGTH), 500, 3*timeleft);
       util_animate_layer(inverter_layer_compat_func_get(s4_bar),  GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN, 0, BARLENGTH), 500, 2*timeleft);
       util_animate_layer(inverter_layer_compat_func_get(s5_bar),  GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, BARWIDTH, BARLENGTH), GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, 0, BARLENGTH), 500, timeleft);
    }
    if ((units_changed & DAY_UNIT) == DAY_UNIT) {
    display_month(t);
    display_day(t);
  }
    
}


  
static void main_window_load(Window *window) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    display_time(t);
    display_month(t);
    display_day(t);
    tick_timer_service_subscribe(SECOND_UNIT, handle_tick);      
    bluetooth_connection_service_subscribe(bt_handler);
    s1_bar = inverter_layer_compat_func_create(GRect(0, 8, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_compat_func_get(s1_bar));
  
    s2_bar = inverter_layer_compat_func_create(GRect(0, 30, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_compat_func_get(s2_bar));
  
    s3_bar = inverter_layer_compat_func_create(GRect(0, 52, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_compat_func_get(s3_bar));
  
    s4_bar = inverter_layer_compat_func_create(GRect(0, 74, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_compat_func_get(s4_bar));
  
    s5_bar = inverter_layer_compat_func_create(GRect(0, 96, 6, 0));
    layer_add_child(window_get_root_layer(window), inverter_layer_compat_func_get(s5_bar));
    //Init seconds bar
    int seconds = t->tm_sec;
        if (seconds > 10){
        util_animate_layer(inverter_layer_compat_func_get(s1_bar), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 0*BARLENGTH + 0*BARMARGIN, BARWIDTH, BARLENGTH), 500, 0); 
        }
        if (seconds > 20){
        util_animate_layer(inverter_layer_compat_func_get(s2_bar), GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 1*BARLENGTH + 1*BARMARGIN, BARWIDTH, BARLENGTH),  500, 100); 
        }  
        if (seconds > 30){
        util_animate_layer(inverter_layer_compat_func_get(s3_bar), GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 2*BARLENGTH + 2*BARMARGIN, BARWIDTH, BARLENGTH), 500, 200); 
        }        

        if (seconds > 40){
        util_animate_layer(inverter_layer_compat_func_get(s4_bar),  GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 3*BARLENGTH + 3*BARMARGIN, BARWIDTH, BARLENGTH), 500, 300); 
        }        
        if (seconds > 50){
        util_animate_layer(inverter_layer_compat_func_get(s5_bar),  GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, 0, BARLENGTH), GRect(0, MARGIN + 4*BARLENGTH + 4*BARMARGIN, BARWIDTH, BARLENGTH), 500, 400);
        }


  
}
static void main_window_unload(Window *window) {
    for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
        unload_digit_image_from_slot(i);
        unload_date_image_from_slot(i);
    }
        unload_day_item();
    inverter_layer_compat_func_destroy(s1_bar);
    inverter_layer_compat_func_destroy(s2_bar);
    inverter_layer_compat_func_destroy(s3_bar);
    inverter_layer_compat_func_destroy(s4_bar);
    inverter_layer_compat_func_destroy(s5_bar);
}
static void init() {
    s_main_window = window_create();
    window_set_background_color(s_main_window, GColorBlack);
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });
    
      fg_color = GColorBlack;
      bg_color = GColorWhite;
      #ifdef PBL_SDK_3
      inverter_layer_compat_set_colors(bg_color, fg_color);
    #endif
    window_stack_push(s_main_window, true);
}
static void deinit() {
    window_destroy(s_main_window);
}
int main(void) {
    init();
    app_event_loop();
    deinit();
}