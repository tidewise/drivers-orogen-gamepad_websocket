/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "GPIOStateWebsocketPublisherTask.hpp"
#include "base-logging/logging/logging_iostream_style.h"
#include "controldev/RawCommand.hpp"
#include "gamepad_websocket/GPIOStateWebsocketPublisherTaskBase.hpp"
#include "rtt/FlowStatus.hpp"

using namespace base;
using namespace controldev;
using namespace gamepad_websocket;
using namespace linux_gpios;

GPIOStateWebsocketPublisherTask::GPIOStateWebsocketPublisherTask(std::string const& name)
    : GPIOStateWebsocketPublisherTaskBase(name)
{
}

GPIOStateWebsocketPublisherTask::~GPIOStateWebsocketPublisherTask()
{
}

bool GPIOStateWebsocketPublisherTask::configureHook()
{
    if (!GPIOStateWebsocketPublisherTaskBase::configureHook())
        return false;
    m_device_identifier = _device_identifier.get();
    m_gpio_state_timeout = _gpio_state_timeout.get();
    return true;
}

bool GPIOStateWebsocketPublisherTask::startHook()
{
    if (!GPIOStateWebsocketPublisherTaskBase::startHook())
        return false;
    m_gpio_state_deadline = Time::now() + m_gpio_state_timeout;
    return true;
}

void GPIOStateWebsocketPublisherTask::updateHook()
{
    GPIOStateWebsocketPublisherTaskBase::updateHook();

    GPIOState gpio_state;
    auto flow_status = _gpio_state.read(gpio_state);
    auto now = Time::now();
    if (flow_status == RTT::NewData) {
        m_gpio_state_deadline = now + m_gpio_state_timeout;
        if (state() != PUBLISHING) {
            state(PUBLISHING);
        }
        updateOutgoingRawCommand(gpio_state);
    }
    else if (now > m_gpio_state_deadline) {
        if (state() != INPUT_TIMEOUT) {
            state(INPUT_TIMEOUT);
        }
        return;
    }
    else if (flow_status == RTT::NoData) {
        return;
    }

    publishRawCommand();
}

void GPIOStateWebsocketPublisherTask::errorHook()
{
    GPIOStateWebsocketPublisherTaskBase::errorHook();
}

void GPIOStateWebsocketPublisherTask::stopHook()
{
    GPIOStateWebsocketPublisherTaskBase::stopHook();
}

void GPIOStateWebsocketPublisherTask::cleanupHook()
{
    GPIOStateWebsocketPublisherTaskBase::cleanupHook();
}

void GPIOStateWebsocketPublisherTask::updateOutgoingRawCommand(
    GPIOState const& gpio_state)
{
    if (!m_outgoing_raw_command.has_value()) {
        m_outgoing_raw_command = RawCommand();
    }

    auto const& state_size = gpio_state.states.size();
    auto const& last_button_count = m_outgoing_raw_command->buttonValue.size();
    if (last_button_count == 0) {
        m_outgoing_raw_command->buttonValue.resize(state_size);
    }
    else if (last_button_count != state_size) {
        LOG_ERROR_S << "Expected a GPIOState with " << last_button_count
                    << " elements, but got one with " << state_size << " elements";
        exception(SIZE_MISMATCH);
        return;
    }

    m_outgoing_raw_command->time = Time::now();
    for (size_t i = 0; i < gpio_state.states.size(); i++) {
        auto const& state = gpio_state.states[i];
        m_outgoing_raw_command->buttonValue[i] = state.data;
    }
}
