/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"
#include "WavAudioSource.h"

#include <windows.h> // for debug

//==============================================================================
MainComponent::MainComponent() :
    menuBar(this),
    mixer(nullptr),
    track_playing_visuals(nullptr),
    track_incoming_visuals(nullptr),
    selected_action(MixScript::SA_MULTIPLY_FADER_GAIN),
    queued_cue(0)
{
    addAndMakeVisible(menuBar);
    
    mixer = std::unique_ptr<MixScript::Mixer>(new MixScript::Mixer());
    track_playing_visuals = std::unique_ptr<MixScript::TrackVisualCache>(new MixScript::TrackVisualCache());
    track_incoming_visuals = std::unique_ptr<MixScript::TrackVisualCache>(new MixScript::TrackVisualCache());

    // Testing
    mixer->LoadPlayingFromFile("C:\\Programming\\MixScript\\mix_script_test_file_juju_outro.wav");
    mixer->LoadIncomingFromFile("C:\\Programming\\MixScript\\mix_script_test_file_martsman.wav");

    // UI
    button_loadfile.setButtonText("...");
    button_loadfile.onClick = [this]() {
        FileChooser chooser("Select Sound File", File::getCurrentWorkingDirectory(), "*.wav;*.aiff");
        if (chooser.browseForFileToOpen())
        {
            const bool paused_state = playback_paused.load();
            playback_paused = true;

            AudioFormatManager format_manager; format_manager.registerBasicFormats();
            const File& file = chooser.getResult();
            mixer->LoadPlayingFromFile(file.getFullPathName().toRawUTF8());
            // TODO: monitor from project
            label_loadfile.setText(file.getFileNameWithoutExtension(), dontSendNotification);            
            track_playing_visuals.get()->peaks.dirty = true;

            playback_paused = paused_state;
        }
    };
    addAndMakeVisible(button_loadfile);
    label_loadfile.setText("[None]", dontSendNotification);
    addAndMakeVisible(label_loadfile);

    button_outfile.setButtonText("...");
    button_outfile.onClick = [this]() {
        FileChooser chooser("Select Sound File", File::getCurrentWorkingDirectory(), "*.wav;*.aiff");
        if (chooser.browseForFileToOpen())
        {
            const bool paused_state = playback_paused.load();
            playback_paused = true;

            AudioFormatManager format_manager; format_manager.registerBasicFormats();
            const File& file = chooser.getResult();
            mixer->LoadIncomingFromFile(file.getFullPathName().toRawUTF8());
            label_outfile.setText(file.getFileNameWithoutExtension(), dontSendNotification);
            track_incoming_visuals.get()->peaks.dirty = true;

            playback_paused = paused_state; // TODO: Scoped pause
        }
    };
    addAndMakeVisible(button_outfile);
    label_outfile.setText("[None]", dontSendNotification);
    addAndMakeVisible(label_outfile);

    visual_accentuate.setButtonText("Accentuate Transients");
    visual_accentuate.onClick = [this]() {
        track_playing_visuals.get()->peaks.SetFilterBypass(!visual_accentuate.getToggleState());
        track_incoming_visuals.get()->peaks.SetFilterBypass(!visual_accentuate.getToggleState());
    };
    addAndMakeVisible(visual_accentuate);

    record_automation.setButtonText("Record Automation");
    record_automation.setToggleState(true, juce::NotificationType::dontSendNotification);
    record_automation.onClick = [this]() {
        mixer->HandleAction(MixScript::SourceActionInfo(MixScript::SA_SET_RECORD, (int)record_automation.getToggleState()));
    };
    addAndMakeVisible(record_automation);

    playing_controls.setBounds(4, 160, playing_controls.getWidth(), playing_controls.getHeight());
    addAndMakeVisible(&playing_controls);
    // TODO: Remove old non-thread safe update.
    playing_controls.on_coefficient_changed = [this](const float gain, const float interpolation_percent) {
        mixer->UpdateGainValue(mixer->Selected(), gain, interpolation_percent);
        SelectedVisuals()->gain_automation.dirty = true;
    };

    playing_controls.on_action = [this](const MixScript::SourceActionInfo& action_info) {
        mixer->HandleAction(action_info);
        SelectedVisuals()->gain_automation.dirty = true;
        // TODO: Must fix gain value in control
    };

    // Make sure you set the size of the component after
    // you add any child components.
    getLookAndFeel().setColour(ResizableWindow::backgroundColourId, Colour(0xAF, 0xAF, 0xAF));
    setSize(1024, 600);

    // specify the number of input and output channels that we want to open
    setAudioChannels (0, 2);

    //addKeyListener(this);
    setWantsKeyboardFocus(true);
    startTimerHz(60);
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
}

void MainComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    // Your audio-processing code goes here!

    // For more details, see the help for AudioProcessor::getNextAudioBlock()

    // Right now we are not producing any data, in which case we need to clear the buffer
    // (to prevent the output of random noise)
    bufferToFill.clearActiveBufferRegion();

    mixer->ProcessActions();

    int32_t cue_pos = queued_cue.load();
    if (cue_pos != 0) {
        queued_cue.compare_exchange_strong(cue_pos, 0); // TODO: don't block on audio thread
        if (cue_pos != 0) {
            mixer->ResetToCue(static_cast<uint32_t>(cue_pos > 0 ? cue_pos : 0));
        }
    }

    if (playback_paused.load()) {
        return;
    }

    MixScript::FloatOutputWriter output_writer = { bufferToFill.buffer->getWritePointer(0),
        bufferToFill.buffer->getWritePointer(1) };
    
    mixer->modifier_mono = modifier_mono.load(); // TODO: make mixer member
    mixer->Mix(output_writer, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

void MainComponent::LoadControls() {
    // TOOD: Get Controls from Mixer
    float interpolation_percent = 0.f;
    const float gain_amount = mixer->FaderGainValue(interpolation_percent);
    playing_controls.LoadControls(gain_amount, interpolation_percent, mixer->Selected().fader_control.bypass,
        juce::NotificationType::dontSendNotification);
    SelectedVisuals()->peaks.dirty = true;
}

bool MainComponent::keyPressed(const KeyPress &key)
{
    const int key_code = key.getKeyCode();    
    bool refresh_controls = false;
    if (key_code >= (int)'0' && key_code <= (int)'9') {
        int cue_id = key_code - (int)'0';
        if (playback_paused &&
            key.getModifiers().isShiftDown()) {
            mixer->SetSelectedMarker(cue_id);
            refresh_controls = true;
        }
        else {
            if (key.getModifiers().isShiftDown()) {
                MixScript::SourceAction prev_action = selected_action;
                switch (key_code) {
                case '1':
                    selected_action = MixScript::SA_MULTIPLY_FADER_GAIN;
                    break;
                case '2':
                    selected_action = MixScript::SA_MULTIPLY_TRACK_GAIN;                    
                    break;
                }
                if (selected_action != prev_action) {
                    track_playing_visuals->gain_automation.dirty = true;
                    track_incoming_visuals->gain_automation.dirty = true;
                }
            }
            else {
                queued_cue = cue_id;
            }
        }
        //return true; // TODO: Handle/not handled
    }
    // Begin Mix Controls
    else if (key_code == 'P') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, 1.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    }
    else if (key_code == '[') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, 3.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    }
    else if (key_code == 'L') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, -1.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    }
    else if (key_code == ']') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, 100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    }
    else if (key_code == '\\') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, -100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    }
    else if (key_code == 'W') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, 1.f, 0 });
        track_playing_visuals->gain_automation.dirty = true;
    }
    else if (key_code == 'A') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, -1.f, 0 });
        track_playing_visuals->gain_automation.dirty = true;
    }
    else if (key_code == 'E') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, 3.f, 0 });
        track_playing_visuals->gain_automation.dirty = true;
    }
    else if (key_code == 'F') {
        if (key.getModifiers().isAltDown()) {
            menuBar.showMenu(0);
        }
        else {
            mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, 100.f, 0 });
            track_playing_visuals->gain_automation.dirty = true;
        }
    }
    else if (key_code == 'G') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, -100.f, 0 });
        track_playing_visuals->gain_automation.dirty = true;
    }
    else if (key_code == 'S') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, 100.f, 0 });
        track_playing_visuals->gain_automation.dirty = true;
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, -100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    }
    else if (key_code == 'K') {
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, -100.f, 0 });
        track_playing_visuals->gain_automation.dirty = true;
        mixer->HandleAction(MixScript::SourceActionInfo{ selected_action, 100.f, 1 });
        track_incoming_visuals->gain_automation.dirty = true;
    }
    else if (key_code == KeyPress::downKey) {
        if (key.getModifiers().isCtrlDown()) {
            SelectedVisuals()->ChangeZoom(-1);
        } else {
            mixer->selected_track = ++mixer->selected_track % 2;
            refresh_controls = true;
        }
    }
    else if (key_code == KeyPress::upKey) {
        if (key.getModifiers().isCtrlDown()) {
            SelectedVisuals()->ChangeZoom(1);
        }
        else {
            mixer->selected_track = ++mixer->selected_track % 2;
            refresh_controls = true;
        }
    }
    else if (key_code == KeyPress::leftKey) {
        if (playback_paused) {
            if (key.getModifiers().isCtrlDown()) {
                mixer->SetSelectedMarker(mixer->MarkerLeft());
                refresh_controls = true;
            }
            else {
                const int32 samples_per_pixel = static_cast<int32>(
                    SelectedVisuals()->SamplesPerPixel(mixer->Selected()));
                mixer->Selected().MoveSelectedMarker(samples_per_pixel> 0 ? -samples_per_pixel : -1);
            }
        }
    }
    else if (key_code == KeyPress::rightKey) {
        if (playback_paused) {
            if (key.getModifiers().isCtrlDown()) {
                mixer->SetSelectedMarker(mixer->MarkerRight());
                refresh_controls = true;
            }
            else {
                const uint32 samples_per_pixel = static_cast<uint32>(
                    SelectedVisuals()->SamplesPerPixel(mixer->Selected()));
                mixer->Selected().MoveSelectedMarker(samples_per_pixel > 0 ? samples_per_pixel : 1);
            }
        }
    }
    else if (key_code == (int)'S') {
        if (playback_paused && key.getModifiers().isShiftDown()) {
            mixer->SetMixSync();
        }
    }
    else if (key_code == (int)' ') {
        const bool set_playback_paused = !playback_paused.load();
        playback_paused = set_playback_paused;
    }
    else if (key_code == KeyPress::homeKey) {
        queued_cue = -1;
    }
    else if (key_code == '=' && key.getModifiers().isShiftDown()) {
        if (playback_paused) {
            if (key.isKeyCurrentlyDown('A')) {
                mixer->AddMarker();
            }
            else {
                mixer->Selected().AddMarker();
            }
        }
    }
    else if (key_code == KeyPress::deleteKey) {
        if (playback_paused) {
            mixer->Selected().DeleteMarker();
        }
    }
    else if (key_code == KeyPress::returnKey && playback_paused) {
        mixer->ResetToCue(mixer->Playing()->selected_marker);
    }
    else if (key_code == 'M' && key.getModifiers().isShiftDown()) {
        modifier_mono = true;
    }
    else if (key_code == (int)'G' && key.getModifiers().isAltDown()) {
        playing_controls.Focus();
    }
    if (refresh_controls) {
        LoadControls();
        return true;
    }
    return false;
}

bool MainComponent::keyStateChanged(bool isKeyDown) {
    if (!isKeyDown) {
        if (modifier_mono) {
            modifier_mono = KeyPress::isKeyCurrentlyDown('M');
            return true;
        }
    }

    return false;
}

MixScript::TrackVisualCache* MainComponent::SelectedVisuals() {
    return mixer->selected_track == 0 ? track_playing_visuals.get() : track_incoming_visuals.get();
}

void MainComponent::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) {
    if (wheel.deltaY != 0.f) {
        if (wheel.deltaY > 0) {
            SelectedVisuals()->ChangeZoom(1);
        }
        else {
            SelectedVisuals()->ChangeZoom(-1);
        }
    }
}

bool HandleMouseDown(MainComponent* mc, const int mouse_x, const int mouse_y, MixScript::Mixer& mixer,
    MixScript::TrackVisualCache& visuals, const int visuals_index, const bool right_click, PopupMenu& menuMarkerType) {
    if (mouse_x >= visuals.draw_region.x &&
        mouse_y >= visuals.draw_region.y &&
        mouse_x <= visuals.draw_region.x + visuals.draw_region.w &&
        mouse_y <= visuals.draw_region.y + visuals.draw_region.h) {
        if (mixer.selected_track != visuals_index) {
            mixer.selected_track = visuals_index;
            mc->LoadControls();
        }
        else if (!visuals.peaks.dirty) {
            const int offset = mouse_x - visuals.draw_region.x;
            const float samples_per_pixel = visuals.SamplesPerPixel(mixer.Selected());
            const auto& format = mixer.Selected().format;
            const int click_offset = (uint32_t)(samples_per_pixel * offset) * format.channels * format.bit_rate / 8;
            uint8_t const * const position = visuals.scroll_offset + click_offset;
            MixScript::ResetToPos(mixer.Selected(), position);
            if (MixScript::Cue* cue = MixScript::TrySelectMarker(mixer.Selected(), position,
                static_cast<int>(2 * samples_per_pixel * format.channels * format.bit_rate / 8))) {
                mc->LoadControls();
                if (right_click) {                   
                    menuMarkerType.clear();
                    menuMarkerType.addItem(MixScript::CT_IMPLIED, "Implied", true, cue->type == MixScript::CT_IMPLIED);
                    menuMarkerType.addItem(MixScript::CT_LEFT, "Region Left", true, cue->type == MixScript::CT_LEFT);
                    menuMarkerType.addItem(MixScript::CT_LEFT_RIGHT, "Region Left/Right", true,
                        cue->type == MixScript::CT_LEFT_RIGHT);
                    menuMarkerType.addItem(MixScript::CT_RIGHT, "Region Right", true, cue->type == MixScript::CT_RIGHT);
                    menuMarkerType.showAt(juce::Rectangle<int>(mouse_x, mouse_y, 10, 22), 0, 0, 0, 0,
                        ModalCallbackFunction::create([cue](const int ret_value) {
                        if (cue->type == ret_value) {
                            cue->type = MixScript::CT_DEFAULT;
                        } else {
                            cue->type = static_cast<MixScript::CueType>(ret_value);
                        }
                    }));
                }
            }
        }
        return true;
    }
    return false;
}

void MainComponent::mouseDown(const MouseEvent &event)
{
    const int mouse_x = event.getMouseDownX();
    const int mouse_y = event.getMouseDownY();
    const bool right_click = event.mods.isRightButtonDown();
    
    // TODO: Clean this up.
    if (!HandleMouseDown(this, mouse_x, mouse_y, *mixer.get(), *track_incoming_visuals.get(), 1, right_click, menuMarkerType)) {
        HandleMouseDown(this, mouse_x, mouse_y, *mixer.get(), *track_playing_visuals.get(), 0, right_click, menuMarkerType);
    }
}

void MainComponent::SaveProject() {
    const bool paused_state = playback_paused.load();
    playback_paused = true;
    FileChooser chooser("Select Output File", juce::File::getCurrentWorkingDirectory(), "*.mix");
    if (chooser.browseForFileToOpen()) {
        const juce::File& file = chooser.getResult().withFileExtension(".mix");        
        mixer->Save(file.getFullPathName().toRawUTF8());
    }
    playback_paused = paused_state;
}

void MainComponent::LoadProject() {
    const bool paused_state = playback_paused.load();
    playback_paused = true;
    FileChooser chooser("Select Project File", juce::File::getCurrentWorkingDirectory(), "*.mix");
    if (chooser.browseForFileToOpen()) {
        const juce::File& file = chooser.getResult().withFileExtension(".mix");
        mixer->Load(file.getFullPathName().toRawUTF8());
        // TODO: Sync visuals state better.
        track_playing_visuals.get()->peaks.dirty = true;
        track_incoming_visuals.get()->peaks.dirty = true;
    }
    playback_paused = paused_state;
}

void MainComponent::ExportRender() {
    const bool paused_state = playback_paused.load();
    playback_paused = true;
    FileChooser chooser("Select Output File", juce::File::getCurrentWorkingDirectory(), "*.wav");
    if (chooser.browseForFileToOpen()) {
        std::unique_ptr<MixScript::WaveAudioSource> output_source = std::unique_ptr<MixScript::WaveAudioSource>(
            std::move(mixer->Render()));
        const juce::File& file = chooser.getResult().withFileExtension(".wav");
        MixScript::WriteWaveFile(file.getFullPathName().toRawUTF8(), output_source);
    }
    playback_paused = paused_state;
}

StringArray MainComponent::getMenuBarNames()
{
    StringArray names;
    names.add("File");
    names.add("Markers");
    return names;
}

enum MenuActions : int {
    MS_Save = 1,
    MS_Load,
    MS_Export,
    MS_Set_Sync,
    MS_Align_Sync,
    MS_Seek_Sync,
    MS_Gen_Implied_Markers
};

PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex, const String& menuName)
{
    PopupMenu menu;

    if (topLevelMenuIndex == 0)
    {
        // TODO: Commands should be managed by the Application in Main.cpp.
        menu.addItem(1, "Save");
        menu.addItem(2, "Load");
        menu.addItem(3, "Export");
    }
    else if (menuName == "Markers") {
        menu.addItem(7, "Generate Implied Markers");
        menu.addSeparator();
        menu.addItem(4, "Set Sync (S)");
        menu.addItem(5, "Align Sync");
        menu.addItem(6, "Seek Sync");        
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) {
    switch (menuItemID)
    {
    case MS_Save:
        SaveProject();
        break;
    case MS_Load:
        LoadProject();
        break;
    case MS_Export:
        ExportRender();
        break;
    case MS_Set_Sync:
        mixer->SetMixSync();
        break;
    case MS_Align_Sync:
        mixer->AlignPlayingSyncToIncomingStart();
        break;
    case MS_Seek_Sync:
        mixer->SeekSync();
        break;
    case MS_Gen_Implied_Markers:
    {
        const bool paused_state = playback_paused.load();
        playback_paused = true;
        mixer->GenerateImpliedMarkers();
        playback_paused = paused_state;
    }
        break;
    default:
        break;
    }
}

void MainComponent::timerCallback() {
    repaint();
}

void PaintAudioSource(Graphics& g, const juce::Rectangle<int>& rect, const MixScript::WaveAudioSource* source, 
        MixScript::TrackVisualCache* track_visuals, const bool selected, const int sync_cue_id, 
        const MixScript::SourceAction selected_action) {
    MixScript::WavePeaks& peaks = track_visuals->peaks;
    MixScript::AmplitudeAutomation& automation = track_visuals->gain_automation;

    juce::Rectangle<int> audio_file_form = rect;
    audio_file_form.reduce(8, 8);
    juce::Rectangle<int> info_bar = audio_file_form.removeFromBottom(12);
    juce::Rectangle<int> markers = audio_file_form.removeFromBottom(12);

    const Colour colour = selected ? Colour::fromRGB(0, 0, 0xAA) : Colour::fromRGB(0x33, 0x33, 0x66);
    const Colour background_color = Colour::fromRGB(0x77, 0x77, selected ? 0xAA : 0x77);
    const Colour midground_colour = selected ? Colour::fromRGB(0x33, 0x33, 0x88) : Colour::fromRGB(0x33, 0x33, 0x55);
    const Colour region_marker_colour = selected ? Colour::fromRGB(0x22, 0x22, 0x22) : Colour::fromRGB(0x55, 0x55, 0x55);

    g.setColour(colour);
    g.drawRect(audio_file_form);
    audio_file_form.reduce(1, 1);

    const uint32 wave_width = audio_file_form.getWidth();
    const uint32 wave_height = audio_file_form.getHeight();
    if (peaks.dirty || peaks.peaks.size() != wave_width) {
        track_visuals->scroll_offset = MixScript::ComputeWavePeaks(*source, wave_width, peaks, track_visuals->zoom_factor);
        track_visuals->draw_region = { audio_file_form.getPosition().x, audio_file_form.getPosition().y,
            static_cast<int32_t>(wave_width), static_cast<int32_t>(wave_height) };
        automation.dirty = true; // TODO: clean-up
        peaks.dirty = false;
    }
    
    g.setFont(10);
    if (source->bpm > 0.f) {
        juce::Rectangle<int> bpm_rect = info_bar.removeFromRight(80);
        const juce::String bpm_label = juce::String::formatted("bpm: %.4f", source->bpm);        
        g.drawText(bpm_label, bpm_rect, juce::Justification::centred);
    }

    g.setColour(background_color);
    // peaks
    {
        int x = audio_file_form.getPosition().x;
        int y = audio_file_form.getPosition().y;
        g.fillRect(x, y + wave_height / 2, wave_width, 1);
        int cue_index = 0;
        uint8_t const * const cursor_offset = source->audio_start + source->last_read_pos;
        for (const MixScript::WavePeaks::WavePeak& peak : peaks.peaks) {
            if (peak.max < peak.min) {
                // TODO: What causes this?
                ++x;
                continue;
            }
            const int line_height = jmax(1 , (int)(wave_height * (peak.max - peak.min) / 2.f));
            g.fillRect(x++, y + wave_height / 2 - int(wave_height * peak.max / 2.f), 1, line_height);
            while (cue_index < source->cue_starts.size() && source->cue_starts[cue_index].start < peak.end) {                
                const MixScript::Cue &cue = source->cue_starts[cue_index];
                if (cue.start >= peak.start) {
                    // Select Cue Color
                    const Colour* marker_color = &colour;
                    if (cue.type != MixScript::CT_DEFAULT && cue.type != MixScript::CT_IMPLIED) {
                        marker_color = &region_marker_colour;
                    }
                    else if (cue_index % 16 != 0) {
                        marker_color = &midground_colour;
                    }
                    g.setColour(*marker_color);
                    const int pixel_pos = x - 1;
                    const int cue_id = cue_index + 1;
                    g.fillRect(pixel_pos, audio_file_form.getTopLeft().y, 1,
                        audio_file_form.getBottom() - audio_file_form.getTopLeft().y);
                    switch (cue.type) {
                    case MixScript::CT_LEFT_RIGHT:
                        g.fillRect(pixel_pos - 8, audio_file_form.getTopLeft().y - 2, 16, 1);
                        g.fillRect(pixel_pos - 8, audio_file_form.getBottom() + 1, 16, 1);
                        break;
                    case MixScript::CT_LEFT:
                        g.fillRect(pixel_pos - 8, audio_file_form.getTopLeft().y - 2, 9, 1);
                        g.fillRect(pixel_pos - 8, audio_file_form.getBottom() + 1, 9, 1);
                        break;
                    case MixScript::CT_RIGHT:
                        g.fillRect(pixel_pos, audio_file_form.getTopLeft().y - 2, 9, 1);
                        g.fillRect(pixel_pos, audio_file_form.getBottom() + 1, 9, 1);
                        break;
                    }
                    if (cue.type != MixScript::CT_IMPLIED || source->selected_marker == cue_id || cue_id == sync_cue_id ||
                        track_visuals->zoom_factor > 1.f) {
                        const juce::String cue_label = juce::String::formatted(cue_id == sync_cue_id ? "%|i" : "%i", cue_id);
                        const int label_width = cue_id > 99 ? 8 : 6;
                        const juce::Rectangle<float> label_rect(pixel_pos - label_width, markers.getPosition().y + 2,
                            label_width * 2, 10);
                        g.drawText(cue_label, label_rect, juce::Justification::centred);
                        if (source->selected_marker == cue_id) {
                            g.drawRoundedRectangle(label_rect.expanded(2), 2.f, 1.f);
                        }
                    }
                    g.setColour(background_color);
                }
                ++cue_index;
            }
            if (cursor_offset >= peak.start && cursor_offset < peak.end) {
                g.setColour(Colour::fromRGB(0xFF, 0xFF, 0xFF));                
                g.fillRect(x - 1, audio_file_form.getTopLeft().y, 1,
                    audio_file_form.getBottom() - audio_file_form.getTopLeft().y);
                g.setColour(background_color);
            }
        }
    }

    // automation
    {
        if (automation.dirty || automation.values.size() != wave_width) {
            MixScript::ComputeParamAutomation(*source, wave_width, automation, track_visuals->zoom_factor,
                track_visuals->scroll_offset, selected_action);
            automation.dirty = false;
        }

        g.setColour(Colour::fromRGB(0xAA, 0x77, 0x33));
        int x = audio_file_form.getPosition().x;
        int y = audio_file_form.getPosition().y;
        int previous_y = -1;
        const int additional_offset = selected_action == MixScript::SA_MULTIPLY_TRACK_GAIN ? wave_height/2 : 0;
        for (const float value : automation.values) {
            const int offset_y = (int)((1.f - value) * wave_height) + 1;
            if (previous_y > 0 && (offset_y ^ previous_y) > 1) {
                g.drawLine(x - 1, y + previous_y + additional_offset, x, y + offset_y + additional_offset);
            }
            g.fillRect(x++, y + offset_y + additional_offset, 1, 1);
            previous_y = offset_y;
        }
    }
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
    const juce::Rectangle<int> bounds = g.getClipBounds();
    const int track_height = 180;
    // Make sure window is not in a strange state.
    if (bounds.getHeight() < track_height * 2 || bounds.getWidth() < 300) {
        return;
    }
    juce::Rectangle<int> audio_file = bounds;
    if (const MixScript::WaveAudioSource* track_incoming = mixer->Incoming()) {
        PaintAudioSource(g, audio_file.removeFromBottom(track_height), track_incoming, track_incoming_visuals.get(),
            mixer->selected_track == 1, mixer->mix_sync.incoming_cue_id, selected_action);
    }
    audio_file.removeFromBottom(2);
    if (const MixScript::WaveAudioSource* track_playing = mixer->Playing()) {
        PaintAudioSource(g, audio_file.removeFromBottom(track_height), track_playing, track_playing_visuals.get(),
            mixer->selected_track == 0, mixer->mix_sync.playing_cue_id, selected_action);
    }
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
    menuBar.setBounds(0, 0, getWidth(), 24);

    juce::Rectangle<int> header_buttons = getLocalBounds();    
    auto row_next = [&header_buttons]() -> decltype(header_buttons)
    {
        const int32 height = 24;
        const int32 padding = 2;
        return header_buttons.removeFromTop(height).reduced(padding);
    };
    row_next();

    juce::Rectangle<int> row_load = row_next();
    button_loadfile.setBounds(row_load.removeFromLeft(36));
    label_loadfile.setBounds(row_load.removeFromLeft(350));

    visual_accentuate.setBounds(row_load.removeFromRight(120));    

    juce::Rectangle<int> row_out = row_next();
    button_outfile.setBounds(row_out.removeFromLeft(36));
    label_outfile.setBounds(row_out.removeFromLeft(350));

    record_automation.setBounds(row_out.removeFromRight(120));
}
