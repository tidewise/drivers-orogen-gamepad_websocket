/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "GamepadCommandAggregatorTask.hpp"

using namespace gamepad_websocket;
using namespace controldev;
using namespace linux_gpios;

GamepadCommandAggregatorTask::GamepadCommandAggregatorTask(std::string const& name)
    : GamepadCommandAggregatorTaskBase(name)
{
}

GamepadCommandAggregatorTask::~GamepadCommandAggregatorTask()
{
}

/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See GamepadCommandAggregatorTask.hpp for more detailed
// documentation about them.

size_t GamepadCommandAggregatorTask::computeTotalAxes()
{
    size_t total_axis;
    for (auto& joystick : m_axis_map) {
        total_axis += joystick.mapped_to_index.size();
    }
    return total_axis;
}
bool GamepadCommandAggregatorTask::configureHook()
{
    if (!GamepadCommandAggregatorTaskBase::configureHook())
        return false;
    m_axis_map = _axis_map.get();
    m_button_map = _button_map.get();
    m_axis.resize(computeTotalAxes());
    m_buttons.resize(m_button_map.size());
    return true;
}
bool GamepadCommandAggregatorTask::startHook()
{
    if (!GamepadCommandAggregatorTaskBase::startHook())
        return false;
    return true;
}

RawCommand GamepadCommandAggregatorTask::aggregateAllCommands()
{
    RawCommand gamepad;
    gamepad.axisValue.resize(m_axis.size());
    gamepad.buttonValue.resize(m_buttons.size());

    for (auto& joystick : m_axis_map) {
        for (auto& axis : joystick.mapped_to_index)
            gamepad.axisValue[axis] = m_axis[axis];
    }

    for (size_t i = 0; i < m_button_map.size(); i++) {
        gamepad.buttonValue[m_button_map[i].mapped_to_index] =
            m_buttons[m_button_map[i].button];
    }

    gamepad.time = base::Time::now();
    return gamepad;
}

void GamepadCommandAggregatorTask::updateHook()
{
    GamepadCommandAggregatorTaskBase::updateHook();

    GPIOState estop;
    if (_gpio_estop_enabled.read(estop) == RTT::NoData) {
        return;
    }
    GPIOState manual_control;
    if (_gpio_manual_control_enabled.read(manual_control) == RTT::NoData) {
        return;
    }
    GPIOState cruise_control;
    if (_gpio_cruise_control_enabled.read(cruise_control) == RTT::NoData) {
        return;
    }
    RawCommand navigation_command;
    if (_navigation_joystick_command.read(navigation_command) == RTT::NoData) {
        return;
    }

    m_buttons[BTN_ESTOP] = estop.states[0].data;
    m_buttons[BTN_MANUAL_CONTROL] = manual_control.states[0].data;
    m_buttons[BTN_CRUISE_CONTROL] = cruise_control.states[0].data;

    m_axis[THRUSTER_COMMAND_AXIS] = navigation_command.axisValue[0];
    m_axis[RUDDER_COMMAND_AXIS] = navigation_command.axisValue[1];

    RawCommand aggregated_command = aggregateAllCommands();
    _aggregated_command.write(aggregated_command);
}
void GamepadCommandAggregatorTask::errorHook()
{
    GamepadCommandAggregatorTaskBase::errorHook();
}
void GamepadCommandAggregatorTask::stopHook()
{
    GamepadCommandAggregatorTaskBase::stopHook();
}
void GamepadCommandAggregatorTask::cleanupHook()
{
    GamepadCommandAggregatorTaskBase::cleanupHook();
}
