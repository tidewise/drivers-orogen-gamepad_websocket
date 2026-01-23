#ifndef gamepad_websocket_TYPES_HPP
#define gamepad_websocket_TYPES_HPP

#include "base/Time.hpp"

namespace gamepad_websocket {
    struct SocketStatistics {
        /* Time of the last sent message, that is the time it was generated */
        base::Time last_sent_message;
        /* Time of the last received message, that is the time it was generated */
        base::Time last_received_message;
        /* Count of messages received */
        uint64_t received = 0;
        /* Count of messages sent */
        uint64_t sent = 0;
    };

    struct Statistics {
        /* Time of generation of this statistics message */
        base::Time time;
        /* The statistics of all currently active sockets */
        std::vector<SocketStatistics> sockets_statistics;
    };
}

#endif
