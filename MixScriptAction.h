// MixScriptAction - list of actions to pass from UI to the Mixer state.
// Author - Nic Taylor

#pragma once

namespace MixScript {
    enum SourceAction : int {
        SA_RESET_AUTOMATION,
        SA_RESET_AUTOMATION_IN_REGION
    };

    struct SourceActionInfo {
        SourceAction action;
        float value;
    };
}