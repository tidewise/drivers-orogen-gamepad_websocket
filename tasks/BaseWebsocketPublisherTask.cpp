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

CommandPublisher::CommandPublisher()
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
    m_publisher = make_shared<CommandPublisher>();
    return true;
}

bool BaseWebsocketPublisherTask::startHook()
{
    if (!BaseWebsocketPublisherTaskBase::startHook())
        return false;

    string endpoint = _endpoint.get();
    uint16_t port = _port.get();
    m_server = nullptr;

    mutex server_thread_ready_mutex;
    condition_variable server_thread_ready_signal;

    bool error = false;
    m_server_thread = async(launch::async,
        [this,
            port,
            endpoint,
            &server_thread_ready_signal,
            &server_thread_ready_mutex,
            &error] {
            auto logger = make_shared<PrintfLogger>(Logger::Level::Debug);
            Server server(logger);

            auto handler = make_shared<WebsocketHandler>();
            handler->m_task = this;
            this->m_publisher->m_handler = handler;
            server.addWebSocketHandler(endpoint.c_str(), handler, true);

            if (!server.startListening(port)) {
                unique_lock<mutex> server_thread_ready_lock(server_thread_ready_mutex);
                error = true;
                server_thread_ready_signal.notify_all();
                return;
            }

            // Needed to terminate in stopHook
            {
                unique_lock<mutex> server_thread_ready_lock(server_thread_ready_mutex);
                this->m_server = &server;
                server_thread_ready_signal.notify_all();
            }

            server.loop();
        });

    {
        unique_lock<mutex> server_thread_ready_lock(server_thread_ready_mutex);
        while (!this->m_server && !error) {
            server_thread_ready_signal.wait(server_thread_ready_lock);
        }
    }
    return !error;
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
