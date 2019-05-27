// MixScriptAction - list of actions to pass from UI to the Mixer state.
// Author - Nic Taylor

#pragma once

#include <vector>
#include <atomic>

namespace MixScript {
    enum SourceAction : int {
        SA_NULL_ACTION,
        SA_UPDATE_GAIN,
        SA_BYPASS_GAIN,
        SA_SET_RECORD,
        SA_RESET_AUTOMATION,
        SA_RESET_AUTOMATION_IN_REGION
    };

    struct SourceActionInfo {
        SourceAction action;
        union {
            float r_value;
            int i_value;
        };

        SourceActionInfo() {
            action = SA_NULL_ACTION;
        }

        SourceActionInfo(const SourceAction _action) {
            action = _action;
        }

        SourceActionInfo(const SourceAction _action, const float _r_value) {
            action = _action;
            r_value = _r_value;
        }

        SourceActionInfo(const SourceAction _action, const int _i_value) {
            action = _action;
            i_value = _i_value;
        }
    };

    class ActionQueue {
    public:
        ActionQueue();
        void WriteAction(const SourceActionInfo& _action);
        void BeginRead();
        bool ReadAction(SourceActionInfo& _action);
    private:
        std::vector<SourceActionInfo> buffer;
        std::atomic<uint32_t> read_index;
        uint32_t read_until;
        std::atomic<uint32_t> write_index;
    };
}