#ifndef gamepad_websocket_TYPES_HPP
#define gamepad_websocket_TYPES_HPP

#include "stdint.h"
#include <cstdint>
#include <vector>

namespace gamepad_websocket {
    enum PhysicalJoystick {
        JS_NAVIGATION,
        JS_MANEUVER
    };

    struct AxisMapping {
        PhysicalJoystick joystick;
        std::vector<uint8_t> mapped_to_index;
    };

    enum PhysicalButton {
        BTN_ESTOP,
        BTN_MANUAL_CONTROL,
        BTN_CRUISE_CONTROL,
        BTN_1,
        BTN_2,
        BTN_3,
        BTN_4,
        BTN_5,
        BTN_6,
        BTN_7,
        BTN_8,
        BTN_9,
        BTN_10,
        BTN_11,
        BTN_12,
        BTN_13,
        BTN_14,
        BTN_15
    };

    struct ButtonMapping {
        PhysicalButton button;
        uint8_t mapped_to_index;
    };
}

#endif
