// Excerpt from main.cpp (P4 Wall Keypad). Not buildable on its own.
//
// Button handling: software debounce plus short-press / long-hold detection,
// polled from the main loop. A long hold fires once while the button is still
// down; releasing after a long hold does not also trigger a short press.

struct ButtonState {
    int last_reading, stable_state;
    int64_t last_change_ms, press_start_ms;
    bool long_hold_fired;
};

static void buttons_update(int idx) {
    int reading = gpio_get_level(BUTTON_PINS[idx]);
    int64_t now = millis();
    ButtonState &btn = buttons[idx];
    if (reading != btn.last_reading) { btn.last_change_ms = now; btn.last_reading = reading; }
    if ((now - btn.last_change_ms) > DEBOUNCE_MS) {
        if (reading != btn.stable_state) {
            btn.stable_state = reading;
            if (btn.stable_state == 0) { btn.press_start_ms = now; btn.long_hold_fired = false; }
            else { if (!btn.long_hold_fired) on_short_press(idx); }
        }
    }
    if (btn.stable_state == 0 && !btn.long_hold_fired && (now - btn.press_start_ms) >= LONG_HOLD_MS) {
        on_long_hold(idx); btn.long_hold_fired = true;
    }
}
