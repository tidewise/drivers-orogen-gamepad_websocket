/* Generated from orogen/lib/orogen/templates/tasks/Task.hpp */

#ifndef GAMEPAD_WEBSOCKET_BASEWEBSOCKETPUBLISHERTASK_TASK_HPP
#define GAMEPAD_WEBSOCKET_BASEWEBSOCKETPUBLISHERTASK_TASK_HPP

#include "Client.hpp"
#include "controldev/RawCommand.hpp"
#include "gamepad_websocket/BaseWebsocketPublisherTaskBase.hpp"

#include <future>
#include <memory>
#include <optional>
#include <seasocks/Server.h>

namespace gamepad_websocket {
    // Forward declaration to avoid circular includes
    class WebsocketHandler;

    /**
     * Implements the interface for a seasocks::Server::Runnable. The #run method of this
     * object is executed in the server thread. This wraps over the WebsocketHandler and
     * run the data publish on the server thread.
     */
    class CommandPublisher : public seasocks::Server::Runnable {
    private:
        std::shared_ptr<WebsocketHandler> m_handler;

    public:
        CommandPublisher(std::shared_ptr<WebsocketHandler> handler);

        void run() override;
    };

    /*! \class BaseWebsocketPublisherTask
     * \brief The task context provides and requires services. It uses an ExecutionEngine
     to perform its functions.
     * Essential interfaces are operations, data flow ports and properties. These
     interfaces have been defined using the oroGen specification.
     * In order to modify the interfaces you should (re)use oroGen and rely on the
     associated workflow.
     * Sets up a websocket serving the given endpoint at the given port
     * \details
     * The name of a TaskContext is primarily defined via:
     \verbatim
     deployment 'deployment_name'
         task('custom_task_name','gamepad_websocket::BaseWebsocketPublisherTask')
     end
     \endverbatim
     *  It can be dynamically adapted when the deployment is called with a prefix
     argument.
     */
    class BaseWebsocketPublisherTask : public BaseWebsocketPublisherTaskBase {
        friend class BaseWebsocketPublisherTaskBase;

    protected:
        std::unique_ptr<seasocks::Server> m_server;
        std::future<void> m_server_thread;
        std::shared_ptr<CommandPublisher> m_publisher;
        std::optional<std::string> m_device_identifier;
        std::optional<controldev::RawCommand> m_outgoing_raw_command;

        /*
         * Requests that the server thread executes the current CommandPublisher
         * in the next cycle.
         */
        void publishRawCommand();

    public:
        /*
         * Returns the latest outgoing raw command to be published to all the
         * clients, if there is one.
         */
        std::optional<controldev::RawCommand> const& outgoingRawCommand();

        std::optional<std::string> const& deviceIdentifier();
        /*
         * Take a list of the active clients statistics and write it in the
         * statistics port. This is called in the server thread by the
         * seasocks::WebSocket::Handler to write only when the statistics change.
         *
         * \param active_clients The clients that are currently active.
         */
        void outputStatistics(std::vector<Client> const& active_clients);

        /** TaskContext constructor for BaseWebsocketPublisherTask
         * \param name Name of the task. This name needs to be unique to make it
         * identifiable via nameservices.
         * \param initial_state The initial TaskState of the TaskContext. Default is
         * Stopped state.
         */
        BaseWebsocketPublisherTask(
            std::string const& name = "gamepad_websocket::BaseWebsocketPublisherTask");

        /** Default deconstructor of BaseWebsocketPublisherTask
         */
        ~BaseWebsocketPublisherTask();

        /** This hook is called by Orocos when the state machine transitions
         * from PreOperational to Stopped. If it returns false, then the
         * component will stay in PreOperational. Otherwise, it goes into
         * Stopped.
         *
         * It is meaningful only if the #needs_configuration has been specified
         * in the task context definition with (for example):
         \verbatim
         task_context "TaskName" do
           needs_configuration
           ...
         end
         \endverbatim
         */
        bool configureHook();

        /** This hook is called by Orocos when the state machine transitions
         * from Stopped to Running. If it returns false, then the component will
         * stay in Stopped. Otherwise, it goes into Running and updateHook()
         * will be called.
         */
        bool startHook();

        /** This hook is called by Orocos when the component is in the Running
         * state, at each activity step. Here, the activity gives the "ticks"
         * when the hook should be called.
         *
         * The error(), exception() and fatal() calls, when called in this hook,
         * allow to get into the associated RunTimeError, Exception and
         * FatalError states.
         *
         * In the first case, updateHook() is still called, and recover() allows
         * you to go back into the Running state.  In the second case, the
         * errorHook() will be called instead of updateHook(). In Exception, the
         * component is stopped and recover() needs to be called before starting
         * it again. Finally, FatalError cannot be recovered.
         */
        void updateHook();

        /** This hook is called by Orocos when the component is in the
         * RunTimeError state, at each activity step. See the discussion in
         * updateHook() about triggering options.
         *
         * Call recover() to go back in the Runtime state.
         */
        void errorHook();

        /** This hook is called by Orocos when the state machine transitions
         * from Running to Stopped after stop() has been called.
         */
        void stopHook();

        /** This hook is called by Orocos when the state machine transitions
         * from Stopped to PreOperational, requiring the call to configureHook()
         * before calling start() again.
         */
        void cleanupHook();
    };
}

#endif
