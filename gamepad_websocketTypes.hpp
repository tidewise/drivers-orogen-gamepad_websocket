#ifndef gamepad_websocket_TYPES_HPP
#define gamepad_websocket_TYPES_HPP

#include "stdint.h"
#include <cstdint>

namespace gamepad_websocket {
    enum PhysicalJoystick {
        JOY_1,
        JOY_2
    };

    struct AxisMapping {
        PhysicalJoystick joystick;
        uint8_t mapped_from_index;
        uint8_t mapped_to_index;
    };

    enum PhysicalButton {
        BTN_MANUAL_CONTROL,
        BNT_SAFETY_MODE
    };

    struct ButtonMapping {
        PhysicalButton button;
        uint8_t mapped_to_index;
        double threshold = 0.5;
    };
}

#endif
