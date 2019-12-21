// MainComponentKeyBindings - Key binding definitions and menu set-up.
// Author - Nic Taylor

#include "MainComponent.h"
#include "MixScriptMixer.h"

void MainComponent::SetUpKeyBindings() {
    key_bindings.clear();
    // Movement Modifiers
    // Incoming
    key_bindings.emplace_back(LightKeyBinding{ (int)'P', juce::String("Incoming +1"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), 1.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'[', juce::String("Incoming +3"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), 3.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'L', juce::String("Incoming -1"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), -1.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)']', juce::String("Incoming Max"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), 100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'\\', juce::String("Incoming Min"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), -100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    // Playing
    key_bindings.emplace_back(LightKeyBinding{ (int)'W', juce::String("Playing +1"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), 1.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'E', juce::String("Playing +3"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), 3.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'A', juce::String("Playing -1"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), -1.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'F', juce::String("Playing Max"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), 100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'G', juce::String("Playing Min"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), -100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'S', juce::String("Playing Only"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), 100.f, 0 });
        track_playing_visuals->gain_automation.dirty = true;
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), -100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'K', juce::String("Incoming Only"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), -100.f, 0 });
        track_playing_visuals->gain_automation.dirty = true;
        mixer->HandleAction(MixScript::SourceActionInfo{ mixer->SelectedAction(), 100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'B', juce::String("Bypass and Solo"), false, false, true,
    [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ MixScript::SA_BYPASS });
        mixer->HandleAction(MixScript::SourceActionInfo{ MixScript::SA_SOLO });
    } });
    // Automation
    key_bindings.emplace_back(LightKeyBinding{ (int)'R', juce::String("Reset Mvmnt in Region"), false, false, false,
        [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo{ MixScript::SA_RESET_AUTOMATION_IN_REGION });
        SelectedVisuals()->gain_automation.dirty = true;
    } });
    // Navigation
    key_bindings.emplace_back(LightKeyBinding{ (int)'F', juce::String("Open Menu"), true, false, false,
        [this]() { menuBar.showMenu(0); } });
    // Tracks
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::downKey, juce::String("Zoom Out"), false, true, false,
        [this]() { SelectedVisuals()->ChangeZoom(-1); } });
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::upKey, juce::String("Zoom In"), false, true, false,
        [this]() { SelectedVisuals()->ChangeZoom(1); } });
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::downKey, juce::String("Select Track Next"), false, false, false,
        [this]() {
        mixer->selected_track = ++mixer->selected_track % 2;
        LoadControls();
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::upKey, juce::String("Select Track Prev"), false, false, false,
        [this]() {
        mixer->selected_track = ++mixer->selected_track % 2;
        LoadControls();
    } });
    // Markers
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::leftKey, juce::String("Select Marker Left"), false, true, false,
        [this]() {
        if (playback_paused) {
            mixer->SetSelectedMarker(mixer->MarkerLeft());
            LoadControls();
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::rightKey, juce::String("Select Marker Right"), false, true, false,
        [this]() {
        if (playback_paused) {
            mixer->SetSelectedMarker(mixer->MarkerRight());
            LoadControls();
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::leftKey, juce::String("Move Marker Left"), false, false, false,
        [this]() {
        if (playback_paused) {
            const int32 samples_per_pixel = static_cast<int32>(
                SelectedVisuals()->SamplesPerPixel(mixer->Selected()));
            mixer->Selected().MoveSelectedMarker(samples_per_pixel> 0 ? -samples_per_pixel : -1);
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::rightKey, juce::String("Move Marker Right"), false, false, false,
        [this]() {
        if (playback_paused) {
            const int32 samples_per_pixel = static_cast<int32>(
                SelectedVisuals()->SamplesPerPixel(mixer->Selected()));
            mixer->Selected().MoveSelectedMarker(samples_per_pixel > 0 ? samples_per_pixel : 1);
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'S', juce::String("Set Mix Sync"), false, false, true,
        [this]() {
        if (playback_paused) {
            mixer->SetMixSync();
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::homeKey, juce::String("Beginning"), false, false, false,
        [this]() {
        queued_cue = -1;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'=', juce::String("Add Marker"), false, false, true,
        [this]() {
        if (playback_paused) {
            mixer->Selected().AddMarker();
            //if (key.isKeyCurrentlyDown('A')) {
            //    mixer->AddMarker();
            //}
            //else {
            //    mixer->Selected().AddMarker();
            //}
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::deleteKey, juce::String("Delete Marker"), false, false, false,
        [this]() {
        if (playback_paused) {
            mixer->Selected().DeleteMarker();
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)KeyPress::returnKey, juce::String("Delete Marker"), false, false, false,
        [this]() {
        if (playback_paused) {
            mixer->ResetToCue(mixer->Playing()->selected_marker);
        }
    } });
    // Playing
    key_bindings.emplace_back(LightKeyBinding{ (int)' ', juce::String("Pause / Unpause"), false, false, false,
        [this]() {
        const bool set_playback_paused = !playback_paused.load();
        playback_paused = set_playback_paused;
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'M', juce::String("Mono Playback"), false, false, true,
        [this]() { mixer->modifier_mono = true; } });
    // Controls
    key_bindings.emplace_back(LightKeyBinding{ (int)'G', juce::String("Focus Gain Control"), true, false, false,
        [this]() { playing_controls.Focus(); } });
    // Movement
    key_bindings.emplace_back(LightKeyBinding{ (int)'1', juce::String("Fader Gain"), true, false, false,
        [this]() {
        const MixScript::SourceAction selected_action = mixer->SelectedAction();
        const MixScript::SourceAction next_action = MixScript::SA_MULTIPLY_FADER_GAIN;
        if (next_action != mixer->SelectedAction()) {
            mixer->SetSelectedAction(next_action);
            track_playing_visuals->gain_automation.dirty = true;
            track_incoming_visuals->gain_automation.dirty = true;
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'2', juce::String("Track Gain"), true, false, false,
        [this]() {
        const MixScript::SourceAction selected_action = mixer->SelectedAction();
        const MixScript::SourceAction next_action = MixScript::SA_MULTIPLY_TRACK_GAIN;
        if (next_action != mixer->SelectedAction()) {
            mixer->SetSelectedAction(next_action);
            track_playing_visuals->gain_automation.dirty = true;
            track_incoming_visuals->gain_automation.dirty = true;
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'3', juce::String("LP Shelf Gain"), true, false, false,
        [this]() {
        const MixScript::SourceAction selected_action = mixer->SelectedAction();
        const MixScript::SourceAction next_action = MixScript::SA_MULTIPLY_LP_SHELF_GAIN;
        if (next_action != mixer->SelectedAction()) {
            mixer->SetSelectedAction(next_action);
            track_playing_visuals->gain_automation.dirty = true;
            track_incoming_visuals->gain_automation.dirty = true;
        }
    } });
    key_bindings.emplace_back(LightKeyBinding{ (int)'4', juce::String("HP Shelf Gain"), true, false, false,
        [this]() {
        const MixScript::SourceAction selected_action = mixer->SelectedAction();
        const MixScript::SourceAction next_action = MixScript::SA_MULTIPLY_HP_SHELF_GAIN;
        if (next_action != mixer->SelectedAction()) {
            mixer->SetSelectedAction(next_action);
            track_playing_visuals->gain_automation.dirty = true;
            track_incoming_visuals->gain_automation.dirty = true;
        }
    } });
}

juce::String KeyCodeToString(int key_code) {
    if (key_code == ' ') {
        return "Space";
    }
    else if (key_code == KeyPress::returnKey) {
        return "Enter";
    }
    else if (key_code == KeyPress::homeKey) {
        return "Home";
    }
    else if (key_code == KeyPress::deleteKey) {
        return "Delete";
    }
    else if (key_code == KeyPress::downKey) {
        return "Down";
    }
    else if (key_code == KeyPress::upKey) {
        return "Up";
    }
    else if (key_code == KeyPress::rightKey) {
        return "Right";
    }
    else if (key_code == KeyPress::leftKey) {
        return "Left";
    }
    return juce::String() + (char)key_code;
}

void MainComponent::ShowKeyBindings() {
    menuKeyBindings.clear();
    int i = 1;
    menuKeyBindings.addSectionHeader("Key Bindings");
    // TODO: Add categories
    for (const LightKeyBinding& key : key_bindings) {
        juce::String modifiers;
        if (key.modifier_alt) {
            modifiers += "Alt + ";
        }
        if (key.modifier_ctrl) {
            modifiers += "Ctrl + ";
        }
        if (key.modifier_shift) {
            modifiers += "Shift + ";
        }
        menuKeyBindings.addItem(i++, key.action_name + ": " + modifiers + KeyCodeToString(key.key_code));
    }
    menuKeyBindings.addSectionHeader("0 .. 9: Playback at Marker");
    menuKeyBindings.addSectionHeader("Shift + 0 .. 9: Select Marker");
    menuKeyBindings.showAt(juce::Rectangle<int>(this->getScreenX() + 10, this->getScreenY() + 10, 0, 0),
        0, 0, 0, 0, nullptr);
}
