# frozen_string_literal: true

require "kontena-websocket-client"
require "json"
require_relative "test_helpers"

using_task_library "gamepad_websocket"

describe OroGen.gamepad_websocket.GPIOStateWebsocketPublisherTask do
    run_live

    attr_reader :task
    attr_reader :websocket

    before do
        @task = task = syskit_deploy(
            OroGen.gamepad_websocket.GPIOStateWebsocketPublisherTask
                  .deployed_as("websocket")
        )
        @port = allocate_interface_port
        task.properties.port = @port
        task.properties.endpoint = "/ws"
        task.properties.gpio_state_timeout = Time.at(1)
        task.properties.device_identifier = "js"

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

    describe "connection diagnostics" do
        before do
            syskit_configure_and_start(task)
        end

        common_connection_diagnostics_tests(self)
    end

    describe "publishing raw commands" do
        before do
            syskit_configure_and_start(task)
            @ws = websocket_create
        end

        it "reflects on the statistics whenever a raw command is published" do
            actual = expect_execution do
                syskit_write task.gpio_state_port, gpio_state([true, false])
            end.to { have_one_new_sample(task.statistics_port) }
            assert_equal 1, actual.sockets_statistics.size

            stats = actual.sockets_statistics.first
            assert_equal 1, stats.sent
        end

        it "reflects on the statistics specific for each socket whenever a raw command " \
           "is published" do
            websocket_create
            actual = expect_execution do
                syskit_write task.gpio_state_port, gpio_state([true, false])
            end.to { have_one_new_sample(task.statistics_port) }
            assert_equal 2, actual.sockets_statistics.size

            actual.sockets_statistics.each do |stats|
                assert_equal 1, stats.sent
            end
        end

        it "stops publishing if no new gpio state sample arrives for gpio state " \
           "timeout time" do
            expect_execution do
                syskit_write task.gpio_state_port, gpio_state([true, false])
            end.to { have_one_new_sample(task.statistics_port) }
            expect_execution.to { emit(task.input_timeout_event) }
            messages_after_timeout = @ws.received_messages

            # Sleep half a second to give a chance for the server to publish messages
            sleep(0.5)
            assert_equal @ws.received_messages, messages_after_timeout
        end

        it "goes into size mismatch if the number of states changes from a sample to " \
           "another" do
            expect_execution do
                syskit_write task.gpio_state_port, gpio_state([true, false])
            end.to { have_one_new_sample(task.statistics_port) }
            expect_execution do
                syskit_write task.gpio_state_port, gpio_state([true, false, true])
            end.to { emit task.size_mismatch_event }
        end

        it "resumes publishing if a new gpio state sample arrives when the task is in " \
           "a input timeout state" do
            expect_execution.to { emit(task.input_timeout_event) }

            expect_execution do
                syskit_write task.gpio_state_port, gpio_state([true, false])
            end.to { have_one_new_sample(task.statistics_port) }

            assert_websocket_receives_message(@ws)
        end

        it "publishes the raw command message in a JSON format" do
            expect_execution do
                syskit_write task.gpio_state_port, gpio_state([true, false, true])
            end.to do
                have_one_new_sample(task.statistics_port)
            end

            msg = assert_websocket_receives_message(@ws)
            msg.delete("timestamp")
            assert_equal({ "axes" => [],
                           "buttons" => [{ "pressed" => true },
                                         { "pressed" => false },
                                         { "pressed" => true }] }, msg)
        end

        it "publishes the raw command message in a JSON format to all connected " \
           "clients" do
            ws2 = websocket_create
            # # Remove the onConnect ID message from the list
            # assert_websocket_receives_message(ws2)

            expect_execution do
                syskit_write task.gpio_state_port, gpio_state([true, false, true])
            end.to do
                have_one_new_sample(task.statistics_port)
            end

            [@ws, ws2].each do |ws_state|
                msg = assert_websocket_receives_message(ws_state)
                msg.delete("timestamp")
                assert_equal({ "axes" => [],
                               "buttons" => [{ "pressed" => true },
                                             { "pressed" => false },
                                             { "pressed" => true }] }, msg)
            end
        end
    end

    def gpio_state(buttons)
        { states: buttons.map { |s| { data: s } } }
    end
end
