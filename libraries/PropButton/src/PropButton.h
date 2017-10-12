#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <ServiceTimer.h>
#include <wiring_digital.h>

#define BUTTON_MAX_EVENTS	5

typedef enum
{
	ButtonActiveLow = 0,
	ButtonActiveHigh
} ButtonType;

typedef enum
{
	ButtonLedActiveLow = 0,
	ButtonLedActiveHigh
} ButtonLedType;

typedef enum
{
	EventNone = 0,
	ButtonPressed,
	ButtonReleased,
	ButtonLongPressed,
	ButtonShortPressAndRelease,
} ButtonEvent;

class PropButton : public STObject
{
public:
	PropButton();
	~PropButton();

	void begin(uint32_t pin, ButtonType type = ButtonActiveHigh, uint32_t minimum_debouncer = 25);
	void end();
	inline bool pressed() { return asserted == 1; }
	inline bool released() { return asserted != 1; }
	inline void setLongPressTime(uint32_t ms) { long_press_time = ms; };
	ButtonEvent getEvent();
	void resetEvents();
	void blink(uint32_t pin, uint32_t cycle_ms, uint32_t time_on, ButtonLedType type = ButtonLedActiveHigh);

protected:
	void poll();
	void addEvent(ButtonEvent event);

private:
	ButtonEvent events[BUTTON_MAX_EVENTS];
	uint8_t event_count;
	uint8_t event_rd;
	uint8_t event_wr;

	volatile uint8_t read_enabled;
	uint8_t enabled;

	volatile uint8_t asserted;
	uint32_t debounce;
	uint32_t pressed_time;
	uint32_t long_press_time;
	uint8_t last_read;
	uint8_t long_press_detected;
	uint32_t counter;
	ButtonType button_type;
	uint32_t button_pin;

	uint8_t blink_enabled;
	uint32_t blink_pin;
	uint32_t blink_cycle;
	uint32_t blink_time_on;
	uint32_t blink_counter;
	ButtonLedType blink_led_type;

	inline void ledOn() {
		digitalWrite(blink_pin, blink_led_type == ButtonLedActiveHigh ? 1 : 0);
	}

	inline void ledOff() {
		digitalWrite(blink_pin, blink_led_type == ButtonLedActiveHigh ? 0 : 1);
	}
};

#endif // __BUTTON_H__
