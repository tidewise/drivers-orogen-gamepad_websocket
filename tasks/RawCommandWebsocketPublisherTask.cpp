/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "RawCommandWebsocketPublisherTask.hpp"
#include "base-logging/Logging.hpp"
#include "controldev/RawCommand.hpp"
#include "gamepad_websocket/RawCommandWebsocketPublisherTaskBase.hpp"
#include "rtt/FlowStatus.hpp"

using namespace base;
using namespace gamepad_websocket;
using namespace std;

RawCommandWebsocketPublisherTask::RawCommandWebsocketPublisherTask(string const& name)
    : RawCommandWebsocketPublisherTaskBase(name)
{
}

RawCommandWebsocketPublisherTask::~RawCommandWebsocketPublisherTask()
{
}

/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See RawCommandWebsocketPublisherTask.hpp for more
// detailed documentation about them.
bool RawCommandWebsocketPublisherTask::configureHook()
{
    if (!RawCommandWebsocketPublisherTaskBase::configureHook())
        return false;

    m_command_timeout = _command_timeout.get();

    auto device_id_transform_str = _device_identifier_transform.get();
    if (!validateDeviceIdTransform(device_id_transform_str)) {
        return false;
    }
    m_device_id_transform = device_id_transform_str;

    return true;
}

bool RawCommandWebsocketPublisherTask::startHook()
{
    if (!RawCommandWebsocketPublisherTaskBase::startHook())
        return false;

    m_command_deadline = Time::now() + m_command_timeout;
    m_device_identifier = {};
    return true;
}

void RawCommandWebsocketPublisherTask::updateHook()
{
    RawCommandWebsocketPublisherTaskBase::updateHook();

    controldev::RawCommand raw_cmd;
    auto flow_status = _raw_command.read(raw_cmd);
    auto now = Time::now();
    if (flow_status == RTT::NewData) {
        m_command_deadline = now + m_command_timeout;
        if (state() != PUBLISHING) {
            state(PUBLISHING);
        }
        lock_guard<mutex> lock(m_shared_data_lock);
        m_outgoing_raw_command = raw_cmd;
    }
    else if (now > m_command_deadline) {
        if (state() != INPUT_TIMEOUT) {
            state(INPUT_TIMEOUT);
        }
        return;
    }
    else if (flow_status == RTT::NoData) {
        return;
    }

    if (!m_device_identifier.has_value()) {
        lock_guard<mutex> lock(m_shared_data_lock);
        m_device_identifier = raw_cmd.deviceIdentifier;
    }
    else if (m_device_identifier.value() != raw_cmd.deviceIdentifier) {
        LOG_ERROR_S << "Detected that a different device was connected. That is not "
                    << "supported. Got " << raw_cmd.deviceIdentifier << " but had "
                    << m_device_identifier.value();
        exception(ID_MISMATCH);
        return;
    }
    publishRawCommand();
}

void RawCommandWebsocketPublisherTask::errorHook()
{
    RawCommandWebsocketPublisherTaskBase::errorHook();
}

void RawCommandWebsocketPublisherTask::stopHook()
{
    RawCommandWebsocketPublisherTaskBase::stopHook();
}

void RawCommandWebsocketPublisherTask::cleanupHook()
{
    RawCommandWebsocketPublisherTaskBase::cleanupHook();
}
