// MixScriptAction - list of actions to pass from UI to the Mixer state.
// Author - Nic Taylor

#pragma once

namespace MixScript {
    enum SourceAction : int {
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
}