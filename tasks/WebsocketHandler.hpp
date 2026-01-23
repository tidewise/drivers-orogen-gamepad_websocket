#ifndef GAMEPAD_WEBSOCKET_WEBSOCKETHANDLER_HPP
#define GAMEPAD_WEBSOCKET_WEBSOCKETHANDLER_HPP

#include "tasks/BaseWebsocketPublisherTask.hpp"

#include <optional>
#include <seasocks/WebSocket.h>

namespace gamepad_websocket {
    /**
     * Implements the interface for a seasocks::WebSocket::Handler that allows one to
     * define the callbacks the server calls for each interaction from a client.
     */
    class WebsocketHandler : public seasocks::WebSocket::Handler {
        void onConnect(seasocks::WebSocket* socket) override;
        void onData(seasocks::WebSocket* socket, const char* data) override;
        void onDisconnect(seasocks::WebSocket* socket) override;

        std::optional<size_t> findSocketIndexFromConnection(
            seasocks::WebSocket* socket) const;

    public:
        /**
         * @brief Publishes the outgoing RawCommand stored in the task to all
         * the active clients. Does nothing when no RawCommand is available yet.
         *
         * The underlying task is responsible for filling the outgoing raw command
         * according to its own interface.
         */
        void publishData();

        /* Pointer to the base task for information shared with *this. */
        BaseWebsocketPublisherTask* m_task;
    };
}

#endif
