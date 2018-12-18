// NRT: JUCE Wrapper for Track Controls parameters.
#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

class TrackControlsComponent : public Component
{
public:
    TrackControlsComponent();

    void LoadControls(const float _gain, const NotificationType notification);

    std::function<void(const float cuttoff_frequency)> on_coefficient_changed;
private:
    void HandleValueChanged(const NotificationType notification);

    Slider slider_gain;
};