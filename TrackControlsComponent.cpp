#include "TrackControlsComponent.h"


TrackControlsComponent::TrackControlsComponent() {
    auto value_changed_func = [this] { HandleValueChanged(sendNotification); };

    const int slider_width = 212;

    slider_gain.setBounds(4, 0, slider_width, 22);
    slider_gain.setRange(0.0, 1.0, 0.01);
    slider_gain.setTextValueSuffix(" Gain");
    slider_gain.setValue(1.0);
    slider_gain.onValueChange = value_changed_func;
    slider_gain.setWantsKeyboardFocus(true); 
    addAndMakeVisible(&slider_gain);

    slider_gain_threshold.setBounds(4 + slider_width, 0, slider_width, 22);
    slider_gain_threshold.setRange(0.0, 1.0, 0.01);
    slider_gain_threshold.setTextValueSuffix(" %");
    slider_gain_threshold.setValue(0.0);
    slider_gain_threshold.onValueChange = value_changed_func;
    slider_gain_threshold.setWantsKeyboardFocus(true);
    addAndMakeVisible(&slider_gain_threshold);

    setSize(2 * (slider_width + 4), 88);

    HandleValueChanged(dontSendNotification);
}

void TrackControlsComponent::Focus() {
    slider_gain.showTextBox();
}

void TrackControlsComponent::LoadControls(const float _gain, const float _interpolation_percent,
    const NotificationType notification) {
    slider_gain.setValue(_gain, notification);
    slider_gain_threshold.setValue(_interpolation_percent, notification);
}

void TrackControlsComponent::HandleValueChanged(const NotificationType notification) {
    if (notification != dontSendNotification)
    {
        if (on_coefficient_changed != nullptr)
        {
            on_coefficient_changed((float)slider_gain.getValue(), (float)slider_gain_threshold.getValue());
        }
    }
}
