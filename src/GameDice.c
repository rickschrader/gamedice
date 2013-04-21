#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "resource_ids.auto.h"
#include "mini-printf.h"

#define MY_UUID { 0x72, 0xC3, 0x7C, 0x97, 0xC9, 0xA0, 0x47, 0x7B, 0x83, 0xCC, 0x8F, 0xD6, 0x07, 0xD5, 0x87, 0xEE }
#define DICE_FONT FONT_KEY_GOTHIC_28_BOLD
#define STATUS_FONT FONT_KEY_GOTHIC_14
#define QUANTITY_TEXT "#d"
#define FACES_TEXT "d#"

PBL_APP_INFO(MY_UUID,
             "Pebble Dice", "Chris Goltz",
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window window;

TextLayer numberLayer;
TextLayer diceLayer;
TextLayer statusLayer;

PropertyAnimation prop_animation_out;
PropertyAnimation prop_animation_in;

long seed;

typedef struct {
	int count;
	int face;
	int value;
	int update;
	} Dice;

Dice d20;
bool updateCountText;
const int faces[] = {3, 4, 6, 8, 10, 12, 20, 100};


void formatDiceString(char *string, Dice die) 
{	
	char *t = "%dd%d";
	mini_snprintf(string, 8, t, die.count, faces[die.face]); 
}

long get_seconds() 
{
    PblTm t;
    get_time(&t);

	// Convert time to seconds since epoch.
   	return t.tm_sec						// start seconds 
		+ t.tm_min*60  					// add minutes 
		+ t.tm_hour*3600  				// add hours 
		+ t.tm_yday*86400  				// add days 
		+ (t.tm_year-70)*31536000  		// add years since 1970 
		+ ((t.tm_year-69)/4)*86400  	// add a day after leap years, starting in 1973 
		- ((t.tm_year-1)/100)*86400 	// remove a leap day every 100 years, starting in 2001 
		+ ((t.tm_year+299)/400)*86400; 	// add a leap day back every 400 years, starting in 2001 
}

char *itoa(int num)
{
	static char buff[20] = {};
	int i = 0, temp_num = num, length = 0;
	char *string = buff;
	
	if(num >= 0) {
		// count how many characters in the number
		while(temp_num) {
			temp_num /= 10;
			length++;
		}
		
		// assign the number to the buffer starting at the end of the 
		// number and going to the begining since we are doing the
		// integer to character comversion on the last number in the
		// sequence
		for(i = 0; i < length; i++) {
		 	buff[(length-1)-i] = '0' + (num % 10);
			num /= 10;
		}
		buff[i] = '\0'; // can't forget the null byte to properly end our string
	}
	else
		return "Unsupported Number";
	
	return string;
}

int random(int max) 
{
	seed = (((seed * 214013L + 2531011L) >> 16) & 32767);

	return ((seed % max) + 1);
}

void number_animation_stopped_handler(Animation *animation, bool finished, void *data)
{
	char *numberText = itoa(d20.value);
	text_layer_set_text(&numberLayer, numberText);

	GRect rect = layer_get_frame(&numberLayer.layer);
	rect.origin.y = 24;
	
	property_animation_init_layer_frame(&prop_animation_in, &numberLayer.layer, NULL, &rect);
	animation_set_duration(&prop_animation_in.animation, 500);
	animation_set_curve(&prop_animation_in.animation, AnimationCurveEaseIn);
	
	animation_schedule(&prop_animation_in.animation);
}

void do_number_animation() {
	GRect rect = layer_get_frame(&numberLayer.layer);
	rect.origin.y = 100;
	
	property_animation_init_layer_frame(&prop_animation_out, &numberLayer.layer, NULL, &rect);
	animation_set_duration(&prop_animation_out.animation, 500);
	animation_set_curve(&prop_animation_out.animation, AnimationCurveEaseOut);
	animation_set_handlers(&prop_animation_out.animation, (AnimationHandlers) {
			.stopped = (AnimationStoppedHandler)number_animation_stopped_handler
		}, &numberLayer);
	animation_schedule(&prop_animation_out.animation);
}

void roll_dice()
{	
	int i = 0;
	d20.value = 0;
	for(i = 0; i < d20.count; i++) {
		d20.value += random(faces[d20.face]);
	}
}

void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) 
{
	if(!updateCountText)
	{
		//text_layer_set_text(&statusLayer, "set quantity");
		text_layer_set_text(&statusLayer, QUANTITY_TEXT);
	}
	else
	{
		//text_layer_set_text(&statusLayer, "set sides");
		text_layer_set_text(&statusLayer, FACES_TEXT);
	}
	updateCountText = !updateCountText;
}

void select_long_click_handler(ClickRecognizerRef recognizer, Window *window) 
{
	roll_dice();
	do_number_animation();
}

void set_diceLayer_text()
{
	char *text = "";
	formatDiceString(text, d20);
	text_layer_set_text(&diceLayer, text);
}

void up_single_click_handler (ClickRecognizerRef recognizer, Window *window) {
	if(updateCountText) {
		d20.count++;
	}
	else {
		if(d20.face + 1 <= 7)
			d20.face++;
	}
	set_diceLayer_text();
}

void down_single_click_handler (ClickRecognizerRef recognizer, Window *window) {
	if(updateCountText) {
		if(d20.count - 1 >= 1)
			d20.count--;
	}
	else {
		if(d20.face - 1 >= 0)
			d20.face--;
	}
	set_diceLayer_text();
}

void config_provider(ClickConfig **config, Window *window) 
{
	(void)window;
	
	// Select button handlers
	config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
	config[BUTTON_ID_SELECT]->long_click.handler = (ClickHandler) select_long_click_handler;

	// Up button handlers
	config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
	config[BUTTON_ID_UP]->click.repeat_interval_ms = 500;
	

	// Down button handlers
	config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
	config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 500;
}

void handle_init(AppContextRef ctx) {
	(void)ctx;
	resource_init_current_app(&DICE_ROLLER_RESOURCES);

	seed = get_seconds();

	updateCountText = true;

	window_init(&window, "diceroller");
	window_stack_push(&window, true /* Animated */);
	
	// Dice layer
	text_layer_init(&diceLayer, GRect(0, 105, 144/* width */, 30/* height */));
	text_layer_set_font(&diceLayer, fonts_get_system_font(DICE_FONT));
	text_layer_set_text_color(&diceLayer, GColorWhite);
	text_layer_set_background_color(&diceLayer, GColorBlack);
	text_layer_set_text_alignment(&diceLayer, GTextAlignmentCenter);
	
	// Status layer
	text_layer_init(&statusLayer, GRect(0, 134, 144/* width */, 18/* height */));
	text_layer_set_font(&statusLayer, fonts_get_system_font(STATUS_FONT));
	text_layer_set_text_color(&statusLayer, GColorWhite);
	text_layer_set_background_color(&statusLayer, GColorBlack);
	text_layer_set_text_alignment(&statusLayer, GTextAlignmentCenter);
	text_layer_set_text(&statusLayer, QUANTITY_TEXT);

	// Number layer
	GFont custom_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MONTSERRAT_NUMBERS_46));
	text_layer_init(&numberLayer, GRect(0, 24, 144/* width */, 70/* height */));
	text_layer_set_font(&numberLayer, custom_font);
	text_layer_set_text_color(&numberLayer, GColorBlack);
	text_layer_set_background_color(&numberLayer, GColorWhite);
	text_layer_set_text_alignment(&numberLayer, GTextAlignmentCenter);
	text_layer_set_text(&numberLayer, "0");

	// Load layers
	layer_add_child(&window.layer, &numberLayer.layer);
	layer_add_child(&window.layer, &diceLayer.layer);
	layer_add_child(&window.layer, &statusLayer.layer);
	
	// Attach our buttons
	window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);
	
	d20.face = 6;
	d20.count = 1;
	d20.value = 0;
	set_diceLayer_text();
}


void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init
		};
	app_event_loop(params, &handlers);
}
