/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "GamepadCommandAggregatorTask.hpp"
#include <algorithm>

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

bool GamepadCommandAggregatorTask::configureHook()
{
    if (!GamepadCommandAggregatorTaskBase::configureHook())
        return false;
    return true;
}
bool GamepadCommandAggregatorTask::startHook()
{
    if (!GamepadCommandAggregatorTaskBase::startHook())
        return false;
    return true;
}

RawCommand GamepadCommandAggregatorTask::aggregateAllCommands(
    RawCommand const& first_joystick,
    RawCommand const& second_joystick,
    GPIOState const& gpios)
{
    RawCommand gamepad;
    // Axis
    gamepad.axisValue.reserve(
        first_joystick.axisValue.size() + second_joystick.axisValue.size());
    gamepad.axisValue.insert(gamepad.axisValue.end(),
        first_joystick.axisValue.begin(),
        first_joystick.axisValue.end());
    gamepad.axisValue.insert(gamepad.axisValue.end(),
        second_joystick.axisValue.begin(),
        second_joystick.axisValue.end());
    // Buttons
    gamepad.buttonValue.resize(gpios.states.size());
    std::transform(gpios.states.begin(),
        gpios.states.end(),
        gamepad.buttonValue.begin(),
        [](const raw_io::Digital& s) { return static_cast<uint8_t>(s.data); });

    gamepad.time = base::Time::now();
    return gamepad;
}

void GamepadCommandAggregatorTask::updateHook()
{
    GamepadCommandAggregatorTaskBase::updateHook();

    GPIOState gpios;
    if (_gpios_command.read(gpios) == RTT::NoData) {
        return;
    }
    RawCommand joystick1_command;
    if (_joystick1_command.read(joystick1_command) == RTT::NoData) {
        return;
    }
    RawCommand joystick2_command;
    _joystick2_command.read(joystick2_command);

    RawCommand aggregated_command =
        aggregateAllCommands(joystick1_command, joystick2_command, gpios);
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
