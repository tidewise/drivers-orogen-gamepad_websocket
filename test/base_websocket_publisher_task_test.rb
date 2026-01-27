# frozen_string_literal: true

require "kontena-websocket-client"
require "json"

using_task_library "gamepad_websocket"

describe OroGen.gamepad_websocket.BaseWebsocketPublisherTask do
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
            OroGen.gamepad_websocket.BaseWebsocketPublisherTask.deployed_as("websocket")
        )
        @port = allocate_interface_port
        task.properties.port = @port
        task.properties.endpoint = "/ws"
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

        it "reports the device identifier when the connection is established" do
            ws = websocket_create
            msg = assert_websocket_receives_message(ws)
            assert_equal({ "id" => "js" }, msg)
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
