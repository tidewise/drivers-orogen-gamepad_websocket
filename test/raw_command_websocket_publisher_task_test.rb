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
                # .deployed_as_unmanaged("websocket")
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

    describe "publishing raw commands" do
        before do
            syskit_configure_and_start(task)
            @ws = websocket_create
        end

        it "reflects on the statistics whenever a raw command is published" do
            actual = expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to { have_one_new_sample(task.statistics_port) }
            assert_equal 1, actual.sockets_statistics.size

            stats = actual.sockets_statistics.first
            assert_equal 1, stats.sent
        end

        it "reflects on the statistics specific for each socket whenever a raw command " \
           "is published" do
            websocket_create
            actual = expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to { have_one_new_sample(task.statistics_port) }
            assert_equal 2, actual.sockets_statistics.size

            actual.sockets_statistics.each do |stats|
                assert_equal 1, stats.sent
            end
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
            assert @ws.received_messages.empty?

            expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to do
                [have_one_new_sample(task.statistics_port),
                 emit(task.publishing_event)]
            end

            assert_websocket_receives_message(@ws)
        end

        it "publishes the raw command message in a JSON format" do
            expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to do
                [have_one_new_sample(task.statistics_port),
                 emit(task.publishing_event)]
            end

            msg = assert_websocket_receives_message(@ws)
            msg = msg.delete("time")
            assert_equal({ axes: [0.5, 1],
                           buttons: [{ pressed: true }, { pressed: false }],
                           id: "js" }, msg)
        end

        it "publishes the raw command message in a JSON format to all connected " \
           "clients" do
            ws2 = websocket_create
            expect_execution do
                syskit_write task.raw_command_port, raw_command([0.5, 1], [1, 0])
            end.to do
                [have_one_new_sample(task.statistics_port),
                 emit(task.publishing_event)]
            end

            [@ws, ws2].each do |ws_state|
                msg = assert_websocket_receives_message(ws_state)
                msg = msg.delete(:time)
                assert_equal({ axes: [0.5, 1],
                               buttons: [{ pressed: true }, { pressed: false }],
                               id: "js" }, msg)
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

    def websocket_create(wait: true)
        s = websocket_state_struct.new(nil, [], nil)
        event = Concurrent::Event.new if wait
        s.connection_thread = Thread.new { websocket_connect(s, event) }

        if event && !event.wait(5)
            unless s.connection_thread.status
                s.connection_thread.value # raises if the thread terminated with exception
            end
            flunk("timed out waiting for the websocket to connect")
        end
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
