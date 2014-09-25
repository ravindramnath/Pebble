#include <pebble.h>
#include <time.h>
#include <ctype.h>
#include "config.h"

static Window *window;

static TextLayer *text_date_layer, *text_time_layer,*text_min_layer,*text_sep_layer, *text_day_layer, *mTemperatureLayer, *mHighLowLayer;
static Layer *mBackgroundLayer;
static Layer *window_layer;
static BitmapLayer *mWeatherIconLayer;
static GBitmap *mWeatherIcon;
static BitmapLayer *background_image;
GBitmap *bg_image;
static int hour;
static int refresh;
static char mTemperatureText[8];
static char mHighLowText[36];

enum {
  WEATHER_TEMPERATURE_KEY = 0,
  WEATHER_CITY_KEY = 1,
};

static uint8_t WEATHER_ICONS[] = {
	RESOURCE_ID_IMAGE_DAY,
	RESOURCE_ID_IMAGE_NIGHT
};

typedef enum {
	WEATHER_ICON_DAY=0,
	WEATHER_ICON_NIGHT=1
} WeatherIcon;

static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin) {
  GBitmap *old_image = *bmp_image;
  *bmp_image = gbitmap_create_with_resource(resource_id);
  GRect frame = (GRect) {
    .origin = origin,
    .size = (*bmp_image)->bounds.size
  };
  bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
  layer_set_frame(bitmap_layer_get_layer(bmp_layer), frame);
  gbitmap_destroy(old_image);
}

void update_background_callback(Layer *me, GContext* ctx) { 
	graphics_context_set_stroke_color(ctx, GColorWhite);
	graphics_draw_line(ctx, GPoint(0, 76), GPoint(144, 76));		
}

void weather_set_icon(WeatherIcon icon) {	
  layer_remove_from_parent(bitmap_layer_get_layer(mWeatherIconLayer));
  mWeatherIconLayer = bitmap_layer_create(WEATHER_ICON_FRAME);  
  set_container_image(&mWeatherIcon, mWeatherIconLayer, WEATHER_ICONS[icon], GPoint(17, 2)); 
	layer_add_child(window_layer, bitmap_layer_get_layer(mWeatherIconLayer));
}

void weather_set_loading() {
	snprintf(mHighLowText, sizeof(mHighLowText), "%s", "South Africa");
  text_layer_set_text_alignment(mHighLowLayer, GTextAlignmentCenter);
	text_layer_set_text(mHighLowLayer, mHighLowText);
  
  time_t now = time(NULL);
  struct tm *ticker_time = localtime(&now);  

  hour = (ticker_time->tm_hour);
  
  if ((hour==18) ||
      (hour==19) ||
      (hour==20) ||
      (hour==21) ||
      (hour==22) ||
      (hour==23) ||
      (hour==0) ||
      (hour==1) ||
      (hour==2) ||
      (hour==3) ||
      (hour==4) ||
      (hour==5)){
    weather_set_icon(1);
    snprintf(mTemperatureText, sizeof(mTemperatureText), "%s\u00B0", "23");
  }
  else{
    weather_set_icon(0);
    snprintf(mTemperatureText, sizeof(mTemperatureText), "%s\u00B0", "30");
  }
 
  text_layer_set_text(mTemperatureLayer, mTemperatureText); 
}

static void in_received_handler(DictionaryIterator *received, void *context) {  
  Tuple *temp;
  temp = dict_find(received, WEATHER_TEMPERATURE_KEY);
  if(temp) {
    text_layer_set_text(mTemperatureLayer, temp->value->cstring);
  }
  Tuple *city;
  city = dict_find(received, WEATHER_CITY_KEY);
  if(city) {
    text_layer_set_text(mHighLowLayer, city->value->cstring);
  }
}

void request_weather(void) {    
  Tuplet mTemp = TupletCString(WEATHER_TEMPERATURE_KEY, "--\u00B0");
  Tuplet mTown = TupletCString(WEATHER_CITY_KEY, "Updating ...");  

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  
  dict_write_tuplet(iter, &mTemp);
  dict_write_tuplet(iter, &mTown);
  dict_write_end(iter);

  app_message_outbox_send();  
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {

  static char time_text[] = "00";
  static char time_text_min[] = "00";
  static char date_text[] = "00 xxx 0000";
  static char day_text[] = "Xxxxxxxxx";
  static char time_text_sep[] = ":";

  char *time_format;
  char *time_format_min;
  
  strftime(date_text, sizeof(date_text), "%d %b %Y", tick_time);
  text_layer_set_text(text_date_layer, date_text);
  
  strftime(day_text, sizeof(day_text), "%A", tick_time);
  text_layer_set_text(text_day_layer, day_text);  

  if (clock_is_24h_style()) {
    time_format = "%H";
    time_format_min = "%M";
  } else {
    time_format = "%I:%M";
  }

  strftime(time_text, sizeof(time_text), time_format, tick_time);
  strftime(time_text_min, sizeof(time_text_min), time_format_min, tick_time);

  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  hour = (tick_time->tm_hour);
  
  if ((hour==18) ||
      (hour==19) ||
      (hour==20) ||
      (hour==21) ||
      (hour==22) ||
      (hour==23) ||
      (hour==0) ||
      (hour==1) ||
      (hour==2) ||
      (hour==3) ||
      (hour==4) ||
      (hour==5)){
    weather_set_icon(1);
    refresh = 30;
  }
  else{
    weather_set_icon(0);
    refresh = 20;
  }
    
  if((tick_time->tm_min % refresh) == 0) {
    request_weather();
  }

  text_layer_set_text(text_time_layer, time_text);
  text_layer_set_text(text_min_layer, time_text_min);
  text_layer_set_text(text_sep_layer, time_text_sep);

}

static void init(void) {
  window = window_create();
  window_set_fullscreen(window, true);
  window_stack_push(window, true);

  bg_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);

  window_layer = window_get_root_layer(window);
	
  GRect bg_bounds = layer_get_frame(window_layer);
  
  app_message_register_inbox_received(in_received_handler); 
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  request_weather();

	background_image = bitmap_layer_create(bg_bounds);
	layer_add_child(window_layer, bitmap_layer_get_layer(background_image));
	bitmap_layer_set_bitmap(background_image, bg_image);  
  
  text_date_layer = text_layer_create(DATE_FRAME);		
  text_layer_set_text_alignment(text_date_layer, GTextAlignmentCenter);
  text_layer_set_text_color(text_date_layer, GColorWhite);
  text_layer_set_background_color(text_date_layer, GColorClear);
  text_layer_set_font(text_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  layer_add_child(window_layer, text_layer_get_layer(text_date_layer));

  text_day_layer = text_layer_create(DAY_FRAME);		
  text_layer_set_text_alignment(text_day_layer, GTextAlignmentCenter);
  text_layer_set_text_color(text_day_layer, GColorWhite);
  text_layer_set_background_color(text_day_layer, GColorClear);  
  text_layer_set_font(text_day_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  layer_add_child(window_layer, text_layer_get_layer(text_day_layer));  

  text_time_layer = text_layer_create(TIME_HOUR_FRAME);
  text_layer_set_text_alignment(text_time_layer, GTextAlignmentCenter);
  text_layer_set_text_color(text_time_layer, GColorWhite);
  text_layer_set_background_color(text_time_layer, GColorClear);
  text_layer_set_text_alignment(text_time_layer, GTextAlignmentRight);
  text_layer_set_font(text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BIG_40)));
  layer_add_child(window_layer, text_layer_get_layer(text_time_layer));

  text_sep_layer = text_layer_create(TIME_SEP_FRAME);    
  text_layer_set_text_alignment(text_sep_layer, GTextAlignmentCenter);
  text_layer_set_text_color(text_sep_layer, GColorWhite);
  text_layer_set_background_color(text_sep_layer, GColorClear);
  text_layer_set_text_alignment(text_sep_layer, GTextAlignmentCenter); 
  text_layer_set_font(text_sep_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BIG_40)));
  layer_add_child(window_layer, text_layer_get_layer(text_sep_layer));  

  text_min_layer = text_layer_create(TIME_MIN_FRAME);
  text_layer_set_text_alignment(text_min_layer, GTextAlignmentCenter);
  text_layer_set_text_color(text_min_layer, GColorWhite);
  text_layer_set_background_color(text_min_layer, GColorClear);
  text_layer_set_text_alignment(text_min_layer, GTextAlignmentLeft); 
  text_layer_set_font(text_min_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BIG_40)));
  layer_add_child(window_layer, text_layer_get_layer(text_min_layer));

  // TEMPERATURE //
  mTemperatureLayer = text_layer_create(WEATHER_TEMP_FRAME);  
  text_layer_set_background_color(mTemperatureLayer, GColorClear);
  text_layer_set_text_color(mTemperatureLayer, GColorWhite);
  text_layer_set_font(mTemperatureLayer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_MEDIUM_34)));
  text_layer_set_text_alignment(mTemperatureLayer, GTextAlignmentCenter);      
  layer_add_child(window_layer, text_layer_get_layer(mTemperatureLayer));

  // HIGHLOW //
  mHighLowLayer = text_layer_create(WEATHER_HL_FRAME);  
  text_layer_set_background_color(mHighLowLayer, GColorClear);
  text_layer_set_text_color(mHighLowLayer, GColorWhite);
  text_layer_set_font(mHighLowLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(mHighLowLayer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(mHighLowLayer));

  // BACKGROUND //
  mBackgroundLayer = layer_create(layer_get_frame(window_layer));
  layer_add_child(window_layer, mBackgroundLayer);
  layer_set_update_proc(mBackgroundLayer, update_background_callback);

  // WEATHER ICON //
  mWeatherIconLayer = bitmap_layer_create(WEATHER_ICON_FRAME);
  layer_add_child(window_layer, bitmap_layer_get_layer(mWeatherIconLayer)); 
  
  weather_set_loading();  
    
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}

static void deinit(void) {	
  app_message_deregister_callbacks();
  text_layer_destroy(text_date_layer);
  text_layer_destroy(text_time_layer);  
  text_layer_destroy(text_day_layer);   
  text_layer_destroy(text_min_layer);  
  text_layer_destroy(mTemperatureLayer); 
  text_layer_destroy(mHighLowLayer);   
  bitmap_layer_destroy(background_image);	
  gbitmap_destroy(bg_image);  	
  tick_timer_service_unsubscribe();	
  window_destroy(window);
}

int main(void) {		
  init();
  app_event_loop();
  deinit();
}
