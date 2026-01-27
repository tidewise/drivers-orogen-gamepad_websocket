/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "BaseWebsocketPublisherTask.hpp"
#include "WebsocketHandler.hpp"
#include "base-logging/Logging.hpp"
#include "gamepad_websocketTypes.hpp"

#include <condition_variable>
#include <memory>
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
    return true;
}

bool BaseWebsocketPublisherTask::startHook()
{
    if (!BaseWebsocketPublisherTaskBase::startHook())
        return false;

    string endpoint = _endpoint.get();
    uint16_t port = _port.get();
    m_server = nullptr;

    auto logger = make_shared<PrintfLogger>(Logger::Level::Debug);
    m_server = make_unique<Server>(logger);

    auto handler = make_shared<WebsocketHandler>(this);
    this->m_publisher = make_shared<CommandPublisher>(handler);
    m_server->addWebSocketHandler(endpoint.c_str(), handler, true);
    if (!m_server->startListening(port)) {
        return false;
    }
    m_server_thread = async(launch::async,
        [this] {
            this->m_server->loop();
        });
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

void BaseWebsocketPublisherTask::outputStatistics()
{
    Statistics stats;
    stats.time = Time::now();
    for (auto socket : m_active_sockets) {
        stats.sockets_statistics.push_back(socket.statistics);
    }

    // Filled by the WebsocketHandler
    _statistics.write(stats);
}

void BaseWebsocketPublisherTask::publishRawCommand()
{
    m_server->execute(m_publisher);
}
