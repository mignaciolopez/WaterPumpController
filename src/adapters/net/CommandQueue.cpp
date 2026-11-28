#include "adapters/net/CommandQueue.h"

namespace CE::Adapters
{
    RestTicket* RestTicket::Create()
    {
        auto* ticket = new RestTicket();
        ticket->_done = xSemaphoreCreateBinary();
        if (ticket->_done == nullptr)
        {
            delete ticket;
            return nullptr;
        }
        return ticket;
    }

    RestTicket::~RestTicket()
    {
        if (_done != nullptr) vSemaphoreDelete(_done);
    }

    void RestTicket::Complete(const Core::ErrorCode result)
    {
        _result = result;
        xSemaphoreGive(_done);
    }

    Core::ErrorCode RestTicket::Wait(const uint32_t timeoutMs)
    {
        if (xSemaphoreTake(_done, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) return Core::ErrorCode::Internal;
        return _result;
    }

    void RestTicket::Release()
    {
        if (_holders.fetch_sub(1) == 1) delete this;
    }

    CommandQueue::~CommandQueue()
    {
        if (_queue != nullptr) vQueueDelete(_queue);
    }

    bool CommandQueue::Begin()
    {
        _queue = xQueueCreate(kLength, sizeof(Incoming));
        return _queue != nullptr;
    }

    bool CommandQueue::Push(const Incoming& incoming)
    {
        return _queue != nullptr && xQueueSend(_queue, &incoming, 0) == pdTRUE;
    }

    bool CommandQueue::Poll(Incoming& out, const uint32_t timeoutMs)
    {
        return _queue != nullptr && xQueueReceive(_queue, &out, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
    }
}
