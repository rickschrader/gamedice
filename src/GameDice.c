#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "resource_ids.auto.h"
#include "mini-printf.h"

#define MY_UUID { 0x72, 0xC3, 0x7C, 0x97, 0xC9, 0xA0, 0x47, 0x7B, 0x83, 0xCC, 0x8F, 0xD6, 0x07, 0xD5, 0x87, 0xEE }
#define DICE_FONT FONT_KEY_GOTHIC_28_BOLD
#define STATUS_FONT FONT_KEY_GOTHIC_14
#define VALUES_FONT FONT_KEY_GOTHIC_14_BOLD
#define NUMBERS_FONT FONT_KEY_GOTHAM_42_BOLD
#define QUANTITY_TEXT "#d"
#define FACES_TEXT "d#"
#define BOUNCE_SPOT 20
#define REST_SPOT 28
#define SCREEN_WIDTH 144
#define SCREEN_HEIGHT 168
#define REPEAT_SPEED 500
#define ANIMATION_SPEED 500
#define MAX_NUMBER_OF_DICE 50

PBL_APP_INFO(MY_UUID,
             "Game Dice", "Chris Goltz",
             1, 3, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_STANDARD_APP);

typedef struct {
	int count;
	int face;
	int value;
	//int values[MAX_NUMBER_OF_DICE];
	char stringValues[5*MAX_NUMBER_OF_DICE]; // '123, ' * max will hold everything possible
} Dice;

Window window;

TextLayer numberLayer;
TextLayer diceLayer;
TextLayer statusLayer;
TextLayer diceValuesLayer;

PropertyAnimation prop_animation_out;
PropertyAnimation prop_animation_in;
PropertyAnimation prop_animation_in2;

Dice displayDie, specialDie, normalDie;
long seed;
bool updateCountText;
const int faces[] = {3, 4, 6, 8, 10, 12, 20, 100};


void formatDiceString(char *string) 
{	
	char *t = "%dd%d";
	mini_snprintf(string, 8, t, normalDie.count, faces[normalDie.face]); 
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
	static char buff[20];
	int i = 0, temp_num = num, length = 0;
	char *string = buff;
	
	if(num >= 0) {
		while(temp_num) {
			temp_num /= 10;
			length++;
		}
		
		for(i = 0; i < length; i++) {
		 	buff[(length-1)-i] = '0' + (num % 10);
			num /= 10;
		}
		buff[i] = '\0';
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

void number_animation_stopped_handler2(Animation *animation, bool finished, void *context)
{
	GRect rect = layer_get_frame(&numberLayer.layer);
	rect.origin.y = REST_SPOT;
		
	property_animation_init_layer_frame(&prop_animation_in2, &numberLayer.layer, NULL, &rect);
	animation_set_duration(&prop_animation_in2.animation, ANIMATION_SPEED);
	animation_set_curve(&prop_animation_in2.animation, AnimationCurveEaseInOut);
	animation_schedule(&prop_animation_in2.animation);
}

void number_animation_stopped_handler1(Animation *animation, bool finished, void *context)
{
	char *numberText = "    ";
	mini_snprintf(numberText, 4, "%d", displayDie.value); 
	text_layer_set_text(&numberLayer, numberText);
	text_layer_set_text(&diceValuesLayer, displayDie.stringValues);
		
	GRect rect = layer_get_frame(&numberLayer.layer);
	rect.origin.y = BOUNCE_SPOT;
	
	property_animation_init_layer_frame(&prop_animation_in, &numberLayer.layer, NULL, &rect);
	animation_set_duration(&prop_animation_in.animation, ANIMATION_SPEED);
	animation_set_curve(&prop_animation_in.animation, AnimationCurveEaseInOut);
	animation_set_handlers(&prop_animation_in.animation, (AnimationHandlers) {
			.stopped = (AnimationStoppedHandler)number_animation_stopped_handler2
		}, &numberLayer);
	animation_schedule(&prop_animation_in.animation);
}

void do_number_animation() {
	GRect rect = layer_get_frame(&numberLayer.layer);
	rect.origin.y = 100;
	
	// There's three options: 1. not set the text layer and have the text update too soon, or
	// 2. not set a null string and have the layer update to the dice string (eg. 1d20) or
	// 3. set a null string, and have it blink out. I can't figure out if 1 and 2 are bugs
	// on my end or the SDK.
	text_layer_set_text(&diceValuesLayer, "\0");
	
	property_animation_init_layer_frame(&prop_animation_out, &numberLayer.layer, NULL, &rect);
	animation_set_duration(&prop_animation_out.animation, ANIMATION_SPEED);
	animation_set_curve(&prop_animation_out.animation, AnimationCurveEaseInOut);
	animation_set_handlers(&prop_animation_out.animation, (AnimationHandlers) {
			.stopped = (AnimationStoppedHandler)number_animation_stopped_handler1
		}, &numberLayer);
	animation_schedule(&prop_animation_out.animation);
}

void roll_dice(Dice* d)
{	
	int i = 0;
	d->value = 0;
	
	memset(&d->stringValues[0], 0, sizeof(d->stringValues)); // clear the array's old elements
	
	for(i = 0; i < d->count; i++) {
		int roll = random(faces[d->face]);
		d->value += roll;
		
		// Build the text for the individual dice
		if(d->count > 1) {
			char *temp = itoa(roll);
			if(i > 0) {
				strcat(d->stringValues, ", ");
			}
			strcat(d->stringValues, temp);
		}
	}
}

void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) 
{
	updateCountText = !updateCountText;
	if(updateCountText)
	{
		text_layer_set_text(&statusLayer, QUANTITY_TEXT);
	}
	else
	{
		text_layer_set_text(&statusLayer, FACES_TEXT);
	}
	
}

void select_multi_click_handler(ClickRecognizerRef recognizer, Window *window) 
{
	roll_dice(&specialDie);
	strcpy(specialDie.stringValues, "Quick 1d20");
	displayDie = specialDie;
	do_number_animation();
}

void select_long_click_handler(ClickRecognizerRef recognizer, Window *window) 
{
	roll_dice(&normalDie);
	displayDie = normalDie;
	do_number_animation();
}

void select_long_release_handler(ClickRecognizerRef recognizer, Window *window) 
{
	// Without this, the "double-click" after a long press is only a single click
}

void set_diceLayer_text()
{
	char *text = "";
	formatDiceString(text);
	text_layer_set_text(&diceLayer, text);
}

void up_single_click_handler (ClickRecognizerRef recognizer, Window *window) 
{
	if(updateCountText) {
		if(normalDie.count + 1 <= MAX_NUMBER_OF_DICE)
			normalDie.count++;
	}
	else {
		if(normalDie.face + 1 <= 7)
			normalDie.face++;
	}
	set_diceLayer_text();
}

void down_single_click_handler (ClickRecognizerRef recognizer, Window *window) 
{
	if(updateCountText) {
		if(normalDie.count - 1 >= 1)
			normalDie.count--;
	}
	else {
		if(normalDie.face - 1 >= 0)
			normalDie.face--;
	}
	set_diceLayer_text();
}

void config_provider(ClickConfig **config, Window *window) 
{
	(void)window;
	
	// Select button handlers
	config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
	config[BUTTON_ID_SELECT]->long_click.handler = (ClickHandler) select_long_click_handler;
	config[BUTTON_ID_SELECT]->multi_click.handler = (ClickHandler) select_multi_click_handler;
	config[BUTTON_ID_SELECT]->multi_click.min = 2;
	config[BUTTON_ID_SELECT]->multi_click.max = 2;
	config[BUTTON_ID_SELECT]->long_click.release_handler = (ClickHandler) select_long_release_handler;

	// Up button handlers
	config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
	config[BUTTON_ID_UP]->click.repeat_interval_ms = REPEAT_SPEED;
	
	// Down button handlers
	config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
	config[BUTTON_ID_DOWN]->click.repeat_interval_ms = REPEAT_SPEED;
}

void handle_init(AppContextRef ctx) {
	(void)ctx;
	resource_init_current_app(&DICE_ROLLER_RESOURCES);

	seed = get_seconds(); // Seed the random numbers

	updateCountText = false; // Set the value to control what gets updated when the up/down buttons are pressed

	window_init(&window, "diceroller");
	window_stack_push(&window, true /* Animated */);
	
	// Dice layer
	text_layer_init(&diceLayer, GRect(0, 105, SCREEN_WIDTH, 30));
	text_layer_set_font(&diceLayer, fonts_get_system_font(DICE_FONT));
	text_layer_set_text_color(&diceLayer, GColorWhite);
	text_layer_set_background_color(&diceLayer, GColorBlack);
	text_layer_set_text_alignment(&diceLayer, GTextAlignmentCenter);
	
	// Status layer
	text_layer_init(&statusLayer, GRect(0, 134, SCREEN_WIDTH, 18));
	text_layer_set_font(&statusLayer, fonts_get_system_font(STATUS_FONT));
	text_layer_set_text_color(&statusLayer, GColorWhite);
	text_layer_set_background_color(&statusLayer, GColorBlack);
	text_layer_set_text_alignment(&statusLayer, GTextAlignmentCenter);
	text_layer_set_text(&statusLayer, FACES_TEXT);

	// Dice values layer
	text_layer_init(&diceValuesLayer, GRect(0, 43, SCREEN_WIDTH, 30));
	text_layer_set_font(&diceValuesLayer, fonts_get_system_font(VALUES_FONT));
	text_layer_set_text_color(&diceValuesLayer, GColorBlack);
	text_layer_set_text_alignment(&diceValuesLayer, GTextAlignmentCenter);
	
	// Number layer
	text_layer_init(&numberLayer, GRect(0, REST_SPOT, SCREEN_WIDTH, 50));
	text_layer_set_font(&numberLayer, fonts_get_system_font(NUMBERS_FONT));
	text_layer_set_text_color(&numberLayer, GColorBlack);
	text_layer_set_text_alignment(&numberLayer, GTextAlignmentCenter);
	text_layer_set_text(&numberLayer, "0");

	// Load layers
	layer_add_child(&window.layer, &numberLayer.layer);
	layer_add_child(&numberLayer.layer, &diceValuesLayer.layer);
	layer_add_child(&window.layer, &diceLayer.layer);
	layer_add_child(&window.layer, &statusLayer.layer);
	layer_set_clips(&numberLayer.layer, false); // allow diceValuesLayer to extend beyond the layer
	
	// Attach our buttons
	window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);
	
	// Set up the dice
	normalDie.face = 6;
	normalDie.count = 1;
	normalDie.value = 0;
	specialDie.face = 6;
	specialDie.count = 1;
	specialDie.value = 0;
	set_diceLayer_text();
}

void handle_deinit(AppContextRef ctx)
{
        (void) ctx;
}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.deinit_handler = &handle_deinit
		};
	app_event_loop(params, &handlers);
}
