# frozen_string_literal: true

using_task_library "gamepad_websocket"
import_types_from "base"
import_types_from "std"
import_types_from "gamepad_websocket"

describe OroGen.gamepad_websocket.GamepadCommandAggregatorTask do
    run_live

    attr_reader :task

    before do
        @task = syskit_deploy(
            OroGen.gamepad_websocket.GamepadCommandAggregatorTask
            .deployed_as("gamepad_aggregator_test")
        )
        now = Time.now
        @joystick1_command = Types.controldev.RawCommand.new(
            time: now,
            axisValue: [0.1, 0.2, -0.3]
        )
        @joystick2_command = Types.controldev.RawCommand.new(
            time: now,
            axisValue: [1.0, 0.5]
        )
        @gpios_cmd = Types.linux_gpios.GPIOState.new(states: [{ data: 1 }, { data: 0 }])

        syskit_configure_and_start(@task)
    end

    it "aggregate all commands: gpios, joystick1, joystick2" do
        syskit_write task.gpios_command_port, @gpios_cmd
        syskit_write task.joystick1_command_port, @joystick1_command
        results =
            expect_execution do
                syskit_write task.joystick2_command_port, @joystick2_command
            end.to do
                have_one_new_sample(task.aggregated_command_port)
            end

        expected_command = Types.controldev.RawCommand.new(
            axisValue: [0.1, 0.2, -0.3, 1.0, 0.5],
            buttonValue: [1, 0]
        )
        assert_equal(expected_command.axisValue, results.axisValue)
        assert_equal(expected_command.buttonValue, results.buttonValue)
    end

    it "does not output the aggregated command if there is no gpios_command" do
        syskit_write task.joystick1_command_port, @joystick1_command
        expect_execution do
            syskit_write task.joystick2_command_port, @joystick2_command
        end.to do
            have_no_new_sample(task.aggregated_command_port)
        end
    end

    it "does not output the aggregated command if there is no joystick1_command" do
        syskit_write task.gpios_command_port, @gpios_cmd
        expect_execution do
            syskit_write task.joystick2_command_port, @joystick2_command
        end.to do
            have_no_new_sample(task.aggregated_command_port)
        end
    end

    it "outputs the aggregated command if there is no joystick2_command" do
        syskit_write task.gpios_command_port, @gpios_cmd
        results =
            expect_execution do
                syskit_write task.joystick1_command_port, @joystick1_command
            end.to do
                have_one_new_sample(task.aggregated_command_port)
            end

        expected_command = Types.controldev.RawCommand.new(
            axisValue: [0.1, 0.2, -0.3],
            buttonValue: [1, 0]
        )
        assert_equal(expected_command.axisValue, results.axisValue)
        assert_equal(expected_command.buttonValue, results.buttonValue)
    end
end
