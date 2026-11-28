#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>
#include <type_traits>

#include "core/Messages.h"

namespace CE::Adapters
{
    /// Lets a REST handler wait for the control task to run its command.
    ///
    /// Two parties hold it (the handler and the control task); whoever lets go last deletes it.
    /// So a handler that gave up waiting can never leave the control task writing into freed memory.
    class RestTicket
    {
    public:
        /// Returns nullptr if the semaphore cannot be created.
        static RestTicket* Create();

        /// Control task: record the result and wake the handler. Then Release().
        void Complete(Core::ErrorCode result);

        /// Handler: waits for Complete(). Returns Internal on timeout.
        Core::ErrorCode Wait(uint32_t timeoutMs);

        /// Each holder calls this exactly once when done.
        void Release();

    private:
        RestTicket() = default;
        ~RestTicket();

        SemaphoreHandle_t _done = nullptr;
        Core::ErrorCode _result = Core::ErrorCode::Internal;
        std::atomic<int> _holders{2};
    };

    /// A command waiting for the control task. `ticket` is set for REST requests.
    struct Incoming
    {
        uint32_t      clientId = 0;      // WebSocket client (unused for REST)
        Core::Command command;
        RestTicket*   ticket = nullptr;
    };

    /// Thread-safe hand-over of commands from the network tasks to the control task.
    class CommandQueue
    {
    public:
        CommandQueue() = default;
        ~CommandQueue();

        CommandQueue(const CommandQueue&) = delete;
        CommandQueue& operator=(const CommandQueue&) = delete;

        [[nodiscard]] bool Begin();

        /// Non-blocking. Returns false when the queue is full.
        bool Push(const Incoming& incoming);

        /// Waits up to `timeoutMs` for the next command.
        bool Poll(Incoming& out, uint32_t timeoutMs);

    private:
        static constexpr size_t kLength = 4;
        static_assert(std::is_trivially_copyable_v<Incoming>, "Incoming is copied through a FreeRTOS queue");

        QueueHandle_t _queue = nullptr;
    };
}
