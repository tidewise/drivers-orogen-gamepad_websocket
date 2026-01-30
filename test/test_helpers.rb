# frozen_string_literal: true

def allocate_interface_port
    server = TCPServer.new(0)
    server.local_address.ip_port
ensure
    server&.close
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
    # until a message with the id is received
    deadline = Time.now + timeout
    while Time.now < deadline
        unless s.received_messages.empty?
            msg = JSON.parse(s.received_messages.shift)
            break if msg["id"] == identifier
        end
        sleep 0.1
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

def common_connection_diagnostics_tests(ctxt) # rubocop:disable Metrics/AbcSize
    ctxt.it "reflects that a connection was established in the statistics port" do
        actual = expect_execution do
            websocket_create
        end.to { have_one_new_sample(task.statistics_port) }
        assert 1, actual.sockets_statistics.size
    end

    ctxt.it "can take multiple peers" do
        websocket_create
        websocket_create
        actual = expect_execution do
            websocket_create
        end.to { have_one_new_sample(task.statistics_port) }
        assert 3, actual.sockets_statistics.size
    end

    ctxt.it "increments the received count for a given client whenever it receives " \
            "data" do
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

    ctxt.it "reflects that a connection is not there anymore in the statistics port" do
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
