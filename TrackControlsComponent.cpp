#include "TrackControlsComponent.h"


TrackControlsComponent::TrackControlsComponent() {
    auto value_changed_func = [this] { HandleValueChanged(sendNotification); };

    const int slider_width = 212;

    slider_gain.setBounds(4, 0, slider_width, 22);
    slider_gain.setRange(0.0, 1.0);
    //slider_temperature.setTextValueSuffix(" F");
    slider_gain.setValue(1.0);
    slider_gain.onValueChange = value_changed_func;
    addAndMakeVisible(&slider_gain);

    //label_cutoff.setText("Cuttoff Freq", dontSendNotification);
    //label_cutoff.setBounds(0, 66, slider_width, 22);
    //addAndMakeVisible(&label_cutoff);

    setSize(slider_width + 4, 88);

    HandleValueChanged(dontSendNotification);
}

void TrackControlsComponent::LoadControls(const float _gain, const NotificationType notification) {
    slider_gain.setValue(_gain, notification);
}

void TrackControlsComponent::HandleValueChanged(const NotificationType notification) {
    const double next_gain = slider_gain.getValue();
    if (notification != dontSendNotification)
    {
        if (on_coefficient_changed != nullptr)
        {
            on_coefficient_changed((float)next_gain);
        }
    }
}
