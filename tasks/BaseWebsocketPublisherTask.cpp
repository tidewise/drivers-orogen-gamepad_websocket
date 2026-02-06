/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "BaseWebsocketPublisherTask.hpp"
#include "WebsocketHandler.hpp"
#include "base-logging/Logging.hpp"
#include "controldev/RawCommand.hpp"
#include "gamepad_websocketTypes.hpp"

#include <memory>
#include <mutex>
#include <seasocks/PrintfLogger.h>
#include <seasocks/Server.h>

using namespace base;
using namespace gamepad_websocket;
using namespace seasocks;
using namespace std;

CommandPublisher::CommandPublisher(shared_ptr<WebsocketHandler> handler)
    : m_handler(handler)
{
}

void CommandPublisher::run()
{
    m_handler->publishData();
}

BaseWebsocketPublisherTask::BaseWebsocketPublisherTask(string const& name)
    : BaseWebsocketPublisherTaskBase(name)
{
}

BaseWebsocketPublisherTask::~BaseWebsocketPublisherTask()
{
}

/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See BaseWebsocketPublisherTask.hpp for more detailed
// documentation about them.

bool BaseWebsocketPublisherTask::configureHook()
{
    if (!BaseWebsocketPublisherTaskBase::configureHook())
        return false;

    m_device_id_transform = "";
    return true;
}

bool BaseWebsocketPublisherTask::startHook()
{
    if (!BaseWebsocketPublisherTaskBase::startHook())
        return false;
    m_outgoing_raw_command = {};

    auto logger = make_shared<PrintfLogger>(Logger::Level::Debug);
    m_server = make_unique<Server>(logger);

    auto handler = make_shared<WebsocketHandler>(this, m_device_id_transform);
    this->m_publisher = make_shared<CommandPublisher>(handler);

    string endpoint = _endpoint.get();
    m_server->addWebSocketHandler(endpoint.c_str(), handler, true);

    uint16_t port = _port.get();
    if (!m_server->startListening(port)) {
        return false;
    }
    m_server_thread = async(launch::async, [this] { this->m_server->loop(); });
    return true;
}

void BaseWebsocketPublisherTask::updateHook()
{
    BaseWebsocketPublisherTaskBase::updateHook();

    auto status = m_server_thread.wait_for(0ms);

    if (status == std::future_status::ready) {
        LOG_ERROR_S << "Server thread unexpectedly terminated" << std::endl;
        exception();
    }
}

void BaseWebsocketPublisherTask::errorHook()
{
    BaseWebsocketPublisherTaskBase::errorHook();
}

void BaseWebsocketPublisherTask::stopHook()
{
    BaseWebsocketPublisherTaskBase::stopHook();

    m_server->terminate();
    m_server_thread.wait();
}

void BaseWebsocketPublisherTask::cleanupHook()
{
    BaseWebsocketPublisherTaskBase::cleanupHook();
}

void BaseWebsocketPublisherTask::outputStatistics(vector<Client> const& active_clients)
{
    Statistics stats;
    stats.time = Time::now();
    for (auto const& socket : active_clients) {
        stats.sockets_statistics.push_back(socket.statistics);
    }
    _statistics.write(stats);
}

void BaseWebsocketPublisherTask::publishRawCommand()
{
    m_server->execute(m_publisher);
}

optional<controldev::RawCommand> BaseWebsocketPublisherTask::outgoingRawCommand()
{
    lock_guard<mutex> lock(m_shared_data_lock);
    return m_outgoing_raw_command;
}

optional<string> BaseWebsocketPublisherTask::deviceIdentifier()
{
    lock_guard<mutex> lock(m_shared_data_lock);
    return m_device_identifier;
}

bool BaseWebsocketPublisherTask::validateDeviceIdTransform(string const& transform_str)
{
    const string token = "%1";
    auto first_pos = transform_str.find(token);
    // No %1 token is a valid transform string
    if (first_pos == string::npos) {
        return true;
    }

    auto second_pos = transform_str.find(token, first_pos + token.length());
    // A single %1 token is a valid transform string
    if (second_pos == string::npos) {
        return true;
    }

    LOG_ERROR_S << "Having more than a %1 token is not supported in the device "
                   "identifier trasnform string";
    return false;
}
