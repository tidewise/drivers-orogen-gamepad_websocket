#include "WebsocketHandler.hpp"
#include "Client.hpp"

#include "base-logging/Logging.hpp"
#include "base-logging/logging/logging_iostream_style.h"
#include "controldev/RawCommand.hpp"

#include <jsoncpp/json/value.h>
#include <jsoncpp/json/writer.h>
#include <seasocks/WebSocket.h>

using namespace base;
using namespace gamepad_websocket;
using namespace seasocks;
using namespace std;

// Declaration and definition in the source file instead of header to avoid dependency
// on jsoncpp
template <typename T>
Json::Value parseArrayCommandToJsonArray(std::vector<T> const& cmds);

template <typename T> Json::Value parseArrayCommandToJsonArray(std::vector<T> const& cmds)
{
    Json::Value array_msg;
    for (auto cmd : cmds) {
        array_msg.append(cmd);
    }
    return array_msg;
}
Json::Value parseRawCommand(controldev::RawCommand const& raw_cmd);

Json::Value parseRawCommand(controldev::RawCommand const& raw_cmd)
{
    Json::Value out_msg;
    out_msg["time"] = static_cast<Json::UInt64>(raw_cmd.time.toMilliseconds());
    out_msg["id"] = raw_cmd.deviceIdentifier;
    out_msg["axes"] = parseArrayCommandToJsonArray(raw_cmd.axisValue);
    out_msg["buttons"] = parseArrayCommandToJsonArray(raw_cmd.buttonValue);
    return out_msg;
}

void WebsocketHandler::onConnect(WebSocket* socket)
{
    Client new_socket;
    new_socket.connection = socket;
    m_task->m_active_sockets.push_back(new_socket);
    m_task->outputStatistics();
}

void WebsocketHandler::onData(WebSocket* socket, const char* data)
{
    auto socket_idx = findSocketIndexFromConnection(socket);
    if (!socket_idx.has_value()) {
        LOG_ERROR_S << "Got data from a connection that is not active!";
        return;
    }
    auto& active_socket = m_task->m_active_sockets[*socket_idx];
    active_socket.statistics.received++;
    active_socket.statistics.last_received_message = Time::now();
    m_task->outputStatistics();
}

void WebsocketHandler::onDisconnect(WebSocket* socket)
{
    auto socket_idx = findSocketIndexFromConnection(socket);
    if (!socket_idx.has_value()) {
        LOG_ERROR_S << "Trying to disconnect a socket that is not active!";
        return;
    }
    m_task->m_active_sockets.erase(m_task->m_active_sockets.begin() + *socket_idx);
    m_task->outputStatistics();
}

void WebsocketHandler::publishData()
{
    if (m_task && !m_task->m_outgoing_raw_command.has_value()) {
        LOG_WARN_S << "Task has no raw command to publish";
        return;
    }

    Json::FastWriter fast;
    auto raw_cmd = *m_task->m_outgoing_raw_command;
    auto msg = parseRawCommand(raw_cmd);
    for (size_t i = 0; i < m_task->m_active_sockets.size(); i++) {
        auto* socket = &m_task->m_active_sockets[i];
        socket->connection->send(fast.write(msg));
        socket->statistics.sent++;
        socket->statistics.last_sent_message = Time::now();
    }
    m_task->outputStatistics();
}

optional<size_t> WebsocketHandler::findSocketIndexFromConnection(WebSocket* socket) const
{
    for (size_t i = 0; i < m_task->m_active_sockets.size(); i++) {
        auto s = m_task->m_active_sockets[i].connection;
        if (s == socket) {
            return i;
        }
    }
    return {};
}
