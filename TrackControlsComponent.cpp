#include "TrackControlsComponent.h"


TrackControlsComponent::TrackControlsComponent() {
    auto value_changed_func = [this] { HandleValueChanged(sendNotification); };    

    int x_offset = 4;
    bypass.onStateChange = [this] {
        on_action(MixScript::SourceActionInfo{ MixScript::SA_BYPASS_GAIN, (int)bypass.getToggleState() });
    };
    bypass.setBounds(x_offset, 0, 21, 21);
    addAndMakeVisible(&bypass);    

    x_offset += 22 + 2;
    const int slider_width = 212;
    slider_gain.setBounds(x_offset, 0, slider_width, 22);
    slider_gain.setRange(0.0, 1.0, 0.01);
    slider_gain.setTextValueSuffix(" Gain");
    slider_gain.setValue(1.0);
    slider_gain.onValueChange = value_changed_func;
    slider_gain.setWantsKeyboardFocus(true); 
    addAndMakeVisible(&slider_gain);

    x_offset += slider_width + 1;
    slider_gain_threshold.setBounds(x_offset, 0, slider_width, 22);
    slider_gain_threshold.setRange(0.0, 1.0, 0.01);
    slider_gain_threshold.setTextValueSuffix(" %");
    slider_gain_threshold.setValue(0.0);
    slider_gain_threshold.onValueChange = [this] {
        on_action(MixScript::SourceActionInfo{ MixScript::SA_UPDATE_GAIN, (float)slider_gain.getValue() });
    };
    slider_gain_threshold.setWantsKeyboardFocus(true);
    addAndMakeVisible(&slider_gain_threshold);

    x_offset += slider_width + 1;
    reset.setButtonText("R");
    reset.onClick = [this] {
        on_action(MixScript::SourceActionInfo{ MixScript::SA_RESET_AUTOMATION });
    };
    reset.setBounds(x_offset, 0, 22, 22);
    addAndMakeVisible(&reset);

    x_offset += 22 + 1;
    reset_in_region.setButtonText("R|");
    reset_in_region.onClick = [this] {
        on_action(MixScript::SourceActionInfo{ MixScript::SA_RESET_AUTOMATION_IN_REGION });
    };
    reset_in_region.setBounds(x_offset, 0, 24, 22);
    addAndMakeVisible(&reset_in_region);

    x_offset += 24 + 1;
    setSize(x_offset + 4, 88);

    HandleValueChanged(dontSendNotification);
}

void TrackControlsComponent::Focus() {
    slider_gain.showTextBox();
}

void TrackControlsComponent::LoadControls(const float _gain, const float _interpolation_percent, const bool _bypass,
    const NotificationType notification) {
    slider_gain.setValue(_gain, notification);
    slider_gain_threshold.setValue(_interpolation_percent, notification);
    bypass.setToggleState(_bypass, notification);
}

void TrackControlsComponent::HandleValueChanged(const NotificationType notification) {
    if (notification != dontSendNotification) {
        if (on_coefficient_changed != nullptr) {
            on_coefficient_changed((float)slider_gain.getValue(), (float)slider_gain_threshold.getValue());
        }
    }
}
