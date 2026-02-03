# frozen_string_literal: true

require "kontena-websocket-client"
require "json"
require_relative "test_helpers"

using_task_library "gamepad_websocket"

describe OroGen.gamepad_websocket.RawCommandWebsocketPublisherTask do
    run_live

    attr_reader :task
    attr_reader :websocket

    before do
        @task = task = syskit_deploy(
            OroGen.gamepad_websocket.RawCommandWebsocketPublisherTask
                .deployed_as("websocket")
        )
        @port = allocate_interface_port
        task.properties.port = @port
        task.properties.endpoint = "/ws"
        task.properties.command_timeout = Time.at(1)

        @url = "ws://127.0.0.1:#{@port}/ws"
        @websocket_created = []
    end

    after do
        @websocket_created.each do |s|
            s.ws.close(10) if s.ws&.open?
            flunk("connection thread failed to quit") unless s.connection_thread.join(10)
        end
    end

    it "stops" do
        syskit_configure_and_start(task)
        expect_execution { task.stop! }
            .to { emit task.interrupt_event }
    end

    it "reports no id when trying to connect when the server doesnt know its " \
       "device identifier" do
        syskit_configure_and_start(task)
        # Assertion is done inside websocket_create
        websocket_create(identifier: nil)
    end

    it "transform the device identifier using the provided transformation" do
        task.properties.device_identifier_transform = "TideWise %1 Joystick"
        syskit_configure_and_start(task)
        write_device_identifier(identifier: "js")
        websocket_create(identifier: "TideWise js Joystick")
    end

    describe "connection diagnostics" do
        before do
            syskit_configure_and_start(task)
            write_device_identifier
        end

        common_connection_diagnostics_tests(self)
    end

    describe "publishing raw commands" do
        before do
            syskit_configure_and_start(task)
            write_device_identifier
            @ws = websocket_create
        end

        it "changes the statistics whenever a raw command is published" do
            before = expect_execution.to { have_one_new_sample(task.statistics_port) }
            before = before.sockets_statistics.first

            actual = expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to { have_one_new_sample(task.statistics_port) }
            assert_equal 1, actual.sockets_statistics.size

            stats = actual.sockets_statistics.first
            assert_operator stats.sent, :>, before.sent
        end

        it "changes the statistics specific for each socket whenever a raw command " \
           "is published" do
            websocket_create

            before = expect_execution.to { have_one_new_sample(task.statistics_port) }
            before = before.sockets_statistics
            actual = expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to { have_one_new_sample(task.statistics_port) }
            assert_equal 2, actual.sockets_statistics.size

            actual.sockets_statistics.zip(before) do |stats, stats_before|
                assert_operator stats.sent, :>, stats_before.sent
            end
        end

        it "go into INPUT MISTMATCH if the sample's device identifier changes" do
            raw_cmd = raw_command([0.5, 1], [1, 0], "macarena")
            syskit_write task.raw_command_port, raw_cmd

            new_raw_cmd = raw_command([0.5, 1], [1, 0], "baila tu cuerpo")
            expect_execution do
                syskit_write task.raw_command_port, new_raw_cmd
            end.to { emit(task.id_mismatch_event) }
        end

        it "stops publishing if no new raw command sample arrives for command timeout " \
           "time" do
            expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to { have_one_new_sample(task.statistics_port) }
            expect_execution.to { emit(task.input_timeout_event) }
            messages_after_timeout = @ws.received_messages

            # Sleep half a second to give a chance for the server to publish messages
            sleep(0.5)
            assert_equal @ws.received_messages, messages_after_timeout
        end

        it "resumes publishing if a new raw command sample arrives when the task is in " \
           "a input timeout state" do
            expect_execution.to { emit(task.input_timeout_event) }

            expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to do
                have_one_new_sample(task.statistics_port)
            end

            assert_websocket_receives_message(@ws)
        end

        it "publishes the raw command message in a JSON format" do
            expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to do
                have_one_new_sample(task.statistics_port)
            end

            msg = assert_websocket_receives_message(@ws)
            msg.delete("timestamp")
            assert_equal({ "axes" => [0.5, 1],
                           "buttons" => [{ "pressed" => true }, { "pressed" => false }] },
                         msg)
        end

        it "publishes the raw command message in a JSON format to all connected " \
           "clients" do
            ws2 = websocket_create
            # Remove the onConnect ID message from the list
            assert_websocket_receives_message(ws2)

            expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to do
                have_one_new_sample(task.statistics_port)
            end

            [@ws, ws2].each do |ws_state|
                msg = assert_websocket_receives_message(ws_state)
                msg.delete("timestamp")
                assert_equal(
                    { "axes" => [0.5, 1],
                      "buttons" => [{ "pressed" => true }, { "pressed" => false }] },
                    msg
                )
            end
        end
    end

    def raw_command(axes, buttons, id = "js")
        { axisValue: axes, buttonValue: buttons, deviceIdentifier: id }
    end

    def write_device_identifier(identifier: "js")
        cmd = { deviceIdentifier: identifier }
        # Write once to ensure the identifier is known
        expect_execution { syskit_write task.raw_command_port, cmd }.to do
            emit task.publishing_event
        end
    end
end
