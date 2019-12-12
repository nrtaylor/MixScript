// MixScriptAction - list of actions to pass from UI to the Mixer state.
// Author - Nic Taylor

#pragma once

#include <vector>
#include <atomic>

namespace MixScript {
    enum SourceAction : int {
        SA_NULL_ACTION,
        SA_UPDATE_GAIN,
        SA_MULTIPLY_FADER_GAIN,
        SA_MULTIPLY_TRACK_GAIN,
        SA_MULTIPLY_LP_SHELF_GAIN,
        SA_MULTIPLY_HP_SHELF_GAIN,
        SA_BYPASS_GAIN,
        SA_SET_RECORD,
        SA_RESET_AUTOMATION,
        SA_RESET_AUTOMATION_IN_REGION,
        SA_BYPASS,
        SA_SOLO,
        SA_CUE_POSITION
    };

    struct SourceActionInfo {
        SourceAction action;
        union {
            float r_value;
            int i_value;
        };
        uint8_t const * position;
        int explicit_target;

        SourceActionInfo() {
            action = SA_NULL_ACTION;
            explicit_target = -1;
        }

        SourceActionInfo(const SourceAction _action) {
            action = _action;
            explicit_target = -1;
        }

        SourceActionInfo(const SourceAction _action, const float _r_value, const int _explicit_target = -1) {
            action = _action;
            r_value = _r_value;
            explicit_target = _explicit_target;
        }

        SourceActionInfo(const SourceAction _action, const int _i_value, const int _explicit_target = -1) {
            action = _action;
            i_value = _i_value;
            explicit_target = _explicit_target;
        }

        SourceActionInfo(const SourceAction _action, uint8_t const * const _position, const int _explicit_target = -1) {
            action = _action;
            position = _position;
            explicit_target = _explicit_target;
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