#ifndef GAMEPAD_WEBSOCKET_CLIENT_HPP
#define GAMEPAD_WEBSOCKET_CLIENT_HPP

#include "gamepad_websocketTypes.hpp"

#include <seasocks/WebSocket.h>

namespace gamepad_websocket {
    /*
     * Represents a websocket client connection with the server alongside the
     * statistics of that connection.
     */
    struct Client {
        seasocks::WebSocket* connection;
        SocketStatistics statistics;
    };
}

#endif
