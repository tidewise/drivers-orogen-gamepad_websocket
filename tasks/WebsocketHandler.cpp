#include "WebsocketHandler.hpp"
#include "BaseWebsocketPublisherTask.hpp"
#include "Client.hpp"

#include "base-logging/Logging.hpp"
#include "controldev/RawCommand.hpp"

#include <algorithm>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/writer.h>
#include <seasocks/WebSocket.h>

using namespace base;
using namespace gamepad_websocket;
using namespace seasocks;
using namespace std;

static Json::Value axesToJson(vector<double> const& cmds)
{
    Json::Value array_msg = Json::arrayValue;
    for (auto cmd : cmds) {
        array_msg.append(cmd);
    }
    return array_msg;
}

static Json::Value buttonsToJson(vector<uint8_t> const& button_cmds)
{
    Json::Value buttons = Json::arrayValue;
    for (auto btn : button_cmds) {
        Json::Value button_json;
        button_json["pressed"] = btn == 1;
        buttons.append(button_json);
    }
    return buttons;
}

static Json::Value rawCommandToJson(controldev::RawCommand const& raw_cmd)
{
    Json::Value out_msg;
    out_msg["timestamp"] = static_cast<Json::UInt64>(raw_cmd.time.toMilliseconds());
    out_msg["axes"] = axesToJson(raw_cmd.axisValue);
    out_msg["buttons"] = buttonsToJson(raw_cmd.buttonValue);
    return out_msg;
}

WebsocketHandler::WebsocketHandler(BaseWebsocketPublisherTask* task,
    string const& device_id_transform)
    : m_task(task)
    , m_device_id_transform(device_id_transform)
{
    if (task == nullptr) {
        throw invalid_argument("WebsocketHandler task cannot be a nullptr");
    }
}

void WebsocketHandler::onConnect(WebSocket* socket)
{
    auto device_identifier = m_task->deviceIdentifier();
    if (!device_identifier.has_value()) {
        Client client;
        client.connection = socket;
        m_pending_sockets.push_back(client);
        return;
    }
    device_identifier = transformDeviceId(device_identifier.value());

    Json::FastWriter writer;
    Json::Value response;
    response["id"] = device_identifier.value();
    socket->send(writer.write(response));

    Client new_socket;
    new_socket.connection = socket;

    m_active_sockets.push_back(new_socket);
    m_task->outputStatistics(m_active_sockets);
}

void WebsocketHandler::onData(WebSocket* socket, const char* data)
{
    auto active_client = clientFromListBySocket(m_active_sockets, socket);
    if (!active_client.has_value()) {
        LOG_ERROR_S << "Got data from a connection that is not active!";
        return;
    }

    (*active_client)->statistics.received++;
    (*active_client)->statistics.last_received_message = Time::now();
    m_task->outputStatistics(m_active_sockets);
}

void WebsocketHandler::onDisconnect(WebSocket* socket)
{
    auto client = clientFromListBySocket(m_active_sockets, socket);
    if (client.has_value()) {
        m_active_sockets.erase(client.value());
        m_task->outputStatistics(m_active_sockets);
        return;
    }
    client = clientFromListBySocket(m_pending_sockets, socket);
    if (client.has_value()) {
        m_pending_sockets.erase(client.value());
        m_task->outputStatistics(m_active_sockets);
        return;
    }

    LOG_ERROR_S << "Trying to disconnect a socket that is not active or pending!";
}

void WebsocketHandler::processPendingPeers()
{
    if (m_pending_sockets.empty()) {
        return;
    }

    auto device_identifier = m_task->deviceIdentifier();
    if (!device_identifier.has_value()) {
        return;
    }
    device_identifier = transformDeviceId(device_identifier.value());

    Json::FastWriter writer;
    Json::Value response;
    response["id"] = device_identifier.value();
    auto json_pkt = writer.write(response);

    while (!m_pending_sockets.empty()) {
        auto& client = m_pending_sockets.back();
        client.connection->send(json_pkt);

        m_active_sockets.push_back(client);
        m_pending_sockets.pop_back();
    }
    m_task->outputStatistics(m_active_sockets);
}

void WebsocketHandler::publishData()
{
    processPendingPeers();
    auto outgoing_raw_command = m_task->outgoingRawCommand();
    if (m_task && !outgoing_raw_command.has_value()) {
        LOG_WARN_S << "Task has no raw command to publish";
        return;
    }

    Json::FastWriter fast;
    auto raw_cmd = outgoing_raw_command.value();
    auto msg = rawCommandToJson(raw_cmd);
    for (auto& socket : m_active_sockets) {
        socket.connection->send(fast.write(msg));
        socket.statistics.sent++;
        socket.statistics.last_sent_message = Time::now();
    }
    m_task->outputStatistics(m_active_sockets);
}

optional<vector<Client>::iterator> WebsocketHandler::clientFromListBySocket(
    vector<Client>& clients_list,
    seasocks::WebSocket* const socket)
{
    auto client_itt = find_if(clients_list.begin(),
        clients_list.end(),
        [socket](Client const& client) { return client.connection == socket; });
    if (client_itt == clients_list.end()) {
        return {};
    }
    return client_itt;
}

string WebsocketHandler::transformDeviceId(string const& device_identifier) const
{
    if (m_device_id_transform.empty()) {
        return device_identifier;
    }

    size_t pos = 0;
    string result = m_device_id_transform;
    if ((pos = m_device_id_transform.find("%1", pos)) != string::npos) {
        result.replace(pos, 2, device_identifier);
    }
    return result;
}
