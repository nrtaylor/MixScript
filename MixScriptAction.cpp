// MixScriptAction - list of actions to pass from UI to the Mixer state.
// Author - Nic Taylor

#include "MixScriptAction.h"

namespace MixScript {
    constexpr uint32_t kBufferSize = 512;
    ActionQueue::ActionQueue() {
        buffer.resize(kBufferSize);
        read_index.store(0);
        write_index.store(0);
        read_until = 0;
    }
    
    void ActionQueue::WriteAction(const SourceActionInfo& _action) {
        SourceActionInfo& action = buffer[write_index.fetch_add(1) % kBufferSize];
        action = _action;
    }
    
    void ActionQueue::BeginRead() {
        read_until = write_index.load();
    }

    bool ActionQueue::ReadAction(SourceActionInfo& _action) {
        const uint32_t current_write_index = write_index.load();
        uint32_t next_read_index = read_index.load();
        if (read_until <= next_read_index || current_write_index <= next_read_index) {
            return false;
        }
        if (read_index.compare_exchange_weak(next_read_index, next_read_index + 1)) {
            _action = buffer[next_read_index % kBufferSize];
        }
    }


}