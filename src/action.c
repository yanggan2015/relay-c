#include "relay_action.h"
#include "util.h"

void relay_action_idle_pulse(int polarity_inverted, int *idle_out, int *pulse_out) {
    if (idle_out) *idle_out = polarity_inverted ? 0 : 1;
    if (pulse_out) *pulse_out = polarity_inverted ? 1 : 0;
}

int relay_logical_to_wire(const RelayConfig *relay, int logical) {
    if (relay && relay->io_inverted) return logical ? 0 : 1;
    return logical ? 1 : 0;
}

int relay_wire_to_logical(const RelayConfig *relay, int wire) {
    if (relay && relay->io_inverted) return wire ? 0 : 1;
    return wire ? 1 : 0;
}

const char *relay_action_mode_name(ActionSequenceMode mode) {
    switch (mode) {
    case ACTION_MODE_PULSE: return "pulse";
    case ACTION_MODE_HOLD_ON: return "hold_on";
    case ACTION_MODE_HOLD_OFF: return "hold_off";
    default: return "pulse";
    }
}

ActionSequenceMode relay_action_mode_parse(const char *s) {
    if (!s || !s[0]) return ACTION_MODE_PULSE;
    if (relay_str_eq_ci(s, "hold_on") || relay_str_eq_ci(s, "on")) return ACTION_MODE_HOLD_ON;
    if (relay_str_eq_ci(s, "hold_off") || relay_str_eq_ci(s, "off")) return ACTION_MODE_HOLD_OFF;
    return ACTION_MODE_PULSE;
}
