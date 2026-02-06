#ifndef GAMEPAD_WEBSOCKET_WEBSOCKETHANDLER_HPP
#define GAMEPAD_WEBSOCKET_WEBSOCKETHANDLER_HPP

#include "BaseWebsocketPublisherTask.hpp"
#include "Client.hpp"

#include <optional>
#include <seasocks/WebSocket.h>

namespace gamepad_websocket {
    /**
     * Implements the interface for a seasocks::WebSocket::Handler that allows one to
     * define the callbacks the server calls for each interaction from a client.
     */
    class WebsocketHandler : public seasocks::WebSocket::Handler {
        std::vector<Client> m_active_sockets;
        std::vector<Client> m_pending_sockets;

        void onConnect(seasocks::WebSocket* socket) override;
        void onData(seasocks::WebSocket* socket, const char* data) override;
        void onDisconnect(seasocks::WebSocket* socket) override;

        void processPendingPeers();
        std::string transformDeviceId(std::string const& device_identifier) const;

        std::optional<std::vector<Client>::iterator> clientFromListBySocket(
            std::vector<Client>& clients_list,
            seasocks::WebSocket* const socket);

    public:
        WebsocketHandler(BaseWebsocketPublisherTask* task = nullptr,
            std::string const& device_id_transform = "");

        /**
         * @brief Publishes the outgoing RawCommand stored in the task to all
         * the active clients. Does nothing when no RawCommand is available yet.
         *
         * The underlying task is responsible for filling the outgoing raw command
         * according to its own interface.
         */
        void publishData();

        /* Pointer to the base task for information shared with *this. */
        BaseWebsocketPublisherTask* m_task = nullptr;

        std::string m_device_id_transform = "";
    };
}

#endif
