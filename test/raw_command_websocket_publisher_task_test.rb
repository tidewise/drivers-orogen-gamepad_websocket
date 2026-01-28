# frozen_string_literal: true

require "kontena-websocket-client"
require "json"

using_task_library "gamepad_websocket"

describe OroGen.gamepad_websocket.RawCommandWebsocketPublisherTask do
    run_live

    attr_reader :task
    attr_reader :websocket

    def allocate_interface_port
        server = TCPServer.new(0)
        server.local_address.ip_port
    ensure
        server&.close
    end

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

    describe "connection diagnostics" do
        before do
            syskit_configure_and_start(task)
            write_device_identifier
        end

        it "reflects that a connection was established in the statistics port" do
            actual = expect_execution do
                websocket_create
            end.to { have_one_new_sample(task.statistics_port) }
            assert 1, actual.sockets_statistics.size
        end

        it "can take multiple peers" do
            websocket_create
            websocket_create
            actual = expect_execution do
                websocket_create
            end.to { have_one_new_sample(task.statistics_port) }
            assert 3, actual.sockets_statistics.size
        end

        it "increments the received count for a given client whenever it receives data" do
            ws = websocket_create
            execute_one_cycle

            before = Time.now
            actual = expect_execution do
                websocket_send(ws, {})
            end.to { have_one_new_sample(task.statistics_port) }
            after = Time.now
            assert 1, actual.sockets_statistics.size
            assert 1, actual.sockets_statistics.first.received
            assert_operator before, :<,
                            actual.sockets_statistics.first.last_received_message
            assert_operator after, :>,
                            actual.sockets_statistics.first.last_received_message
        end

        it "reflects that a connection is not there anymore in the statistics port" do
            ws = websocket_create
            actual = expect_execution do
                websocket_create
            end.to { have_one_new_sample(task.statistics_port) }
            assert 1, actual.sockets_statistics.size

            actual = expect_execution do
                websocket_disconnect(ws)
            end.to { have_one_new_sample(task.statistics_port) }
            assert 0, actual.sockets_statistics.size
        end
    end

    describe "publishing raw commands" do
        before do
            syskit_configure_and_start(task)
            write_device_identifier
            @ws = websocket_create
            # Receives the connection message
            assert_websocket_receives_message(@ws)
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
            msg.delete("time")
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
                msg.delete("time")
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

    def websocket_state_struct
        @websocket_state_struct ||= Concurrent::MutableStruct.new(
            :ws, :received_messages, :connection_thread
        )
    end

    def websocket_send(state, msg)
        msg = JSON.generate(msg) unless msg.respond_to?(:to_str)
        state.ws.send(msg)
    end

    def websocket_connect(state, event)
        Kontena::Websocket::Client.connect(@url) do |ws|
            state.ws = ws
            event&.set
            ws.read do |message|
                state.received_messages += [message]
            end
        end
    rescue Kontena::Websocket::CloseError
    ensure
        event&.set
    end

    def write_device_identifier(identifier: "js")
        cmd = { deviceIdentifier: identifier }
        # Write once to ensure the identifier is known
        expect_execution { syskit_write task.raw_command_port, cmd }.to do
            emit task.publishing_event
        end
    end

    def websocket_create(wait: true, identifier: "js", timeout: 3)
        s = websocket_state_struct.new(nil, [], nil)
        event = Concurrent::Event.new if wait
        s.connection_thread = Thread.new { websocket_connect(s, event) }

        if event && !event.wait(5)
            unless s.connection_thread.status
                s.connection_thread.value # raises if the thread terminated with exception
            end
            flunk("timed out waiting for the websocket to connect")
        end
        # Connect can happen at the same time as the publish execution. This block waits
        # until any message is received and takes the FIRST, instead of the LAST which is
        # done by #assert_websocket_receives_message
        deadline = Time.now + timeout
        while Time.now < deadline
            unless s.received_messages.empty?
                msg = JSON.parse(s.received_messages.first)
                break
            end
            sleep 0.1
        end
        assert_equal({ "id" => identifier }, msg)
        @websocket_created << s

        s
    end

    def websocket_disconnect(state)
        ws = state.ws
        ws.close
    end

    def assert_websocket_receives_message(state, timeout: 3)
        deadline = Time.now + timeout
        while Time.now < deadline
            unless state.received_messages.empty?
                return JSON.parse(state.received_messages.pop)
            end

            sleep 0.1
        end
        flunk("no messages received in #{timeout}s")
    end
end
