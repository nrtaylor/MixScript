// NRT: JUCE Wrapper for Track Controls parameters.
#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "MixScriptAction.h"

class TrackControlsComponent : public Component
{
public:
    TrackControlsComponent();

    void LoadControls(const float _gain, const float _interpolation_percent, const bool _bypass,
        const NotificationType notification);

    std::function<void(const float _gain, const float _interpolation_percent,
        const bool _bypass)> on_coefficient_changed;

    std::function<void(const MixScript::SourceActionInfo& action_info)> on_action;

    void Focus();
private:
    void HandleValueChanged(const NotificationType notification);

    Slider slider_gain;
    Slider slider_gain_threshold;
    ToggleButton bypass;
    TextButton reset;
    TextButton reset_in_region;
};