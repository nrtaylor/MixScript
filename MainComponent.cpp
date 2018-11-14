/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"
#include "WavAudioSource.h"

//==============================================================================
MainComponent::MainComponent() :
    menuBar(this),
    mixer(nullptr),
    queued_cue(0)
{
    addAndMakeVisible(menuBar);
    
    mixer = std::unique_ptr<MixScript::Mixer>(new MixScript::Mixer());

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

            playback_paused = paused_state; // TODO: Scoped pause
        }
    };
    addAndMakeVisible(button_outfile);
    label_outfile.setText("[None]", dontSendNotification);
    addAndMakeVisible(label_outfile);

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

bool MainComponent::keyPressed(const KeyPress &key)
{
    const int key_code = key.getKeyCode();
    if (key_code >= (int)'0' && key_code <= (int)'9') {
        queued_cue = key_code - (int)'0';
        return true;
    }
    else if (key_code == (int)'R') {
        ExportRender();
    }
    else if (key_code == (int)' ') {
        const bool set_playback_paused = !playback_paused.load();
        playback_paused = set_playback_paused;
    }
    else if (key_code == KeyPress::homeKey) {
        queued_cue = -1;
    }
    else if (key_code == 'M' && key.getModifiers().isShiftDown()) {
        modifier_mono = true;
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
    return names;
}

PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex, const String& /*menuName*/)
{
    PopupMenu menu;

    if (topLevelMenuIndex == 0)
    {
        // TODO: Commands should be managed by the Application in Main.cpp.
        menu.addItem(1, "Save");
        menu.addItem(2, "Load");
        menu.addItem(3, "Export");
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) {
    switch (menuItemID)
    {
    case 1:
        SaveProject();
        break;
    case 2:
        LoadProject();
        break;
    case 3:
        ExportRender();
        break;
    default:
        break;
    }
}

void MainComponent::timerCallback() {
    repaint();
}

void PaintAudioSource(Graphics& g, const Rectangle<int>& rect, const MixScript::WaveAudioSource* source) {
    Rectangle<int> audio_file_form = rect;
    audio_file_form.reduce(12, 12);
    Rectangle<int> markers = audio_file_form.removeFromBottom(12);

    g.setColour(Colour::fromRGB(0, 0, 0x88));
    g.drawRect(audio_file_form);
    audio_file_form.reduce(1, 1);

    g.setFont(10);
    uint8_t* const audio_start = source->audio_start;
    const float inv_duration = 1.f / (float)(source->audio_end - audio_start);
    int cue_id = 1;
    for (const uint8_t* cue_pos : source->cue_starts) {
        const float ratio = (cue_pos - audio_start) * inv_duration;
        const int pixel_pos = static_cast<int>(ratio * audio_file_form.getWidth()) + audio_file_form.getPosition().x;
        g.drawLine(pixel_pos, audio_file_form.getBottom(), pixel_pos, audio_file_form.getTopLeft().y, 1.f);
        g.drawText(juce::String::formatted("%i", cue_id),
            pixel_pos - 6,
            markers.getPosition().y + 1,
            12,
            10,
            juce::Justification::centred);
        ++cue_id;
    }
    g.setColour(Colour::fromRGB(0xFF, 0xFF, 0xFF));
    const float play_pos_ratio = source->last_read_pos * inv_duration;
    const int play_pos = static_cast<int>(play_pos_ratio * audio_file_form.getWidth()) + audio_file_form.getPosition().x;
    g.drawLine(play_pos, audio_file_form.getBottom(), play_pos, audio_file_form.getTopLeft().y);
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
    const Rectangle<int> bounds = g.getClipBounds();
    Rectangle<int> audio_file = bounds;
    //Rectangle<int> audio_file_form = audio_file.removeFromBottom(120);
    if (const MixScript::WaveAudioSource* track_incoming = mixer->Incoming()) {
        PaintAudioSource(g, audio_file.removeFromBottom(120), track_incoming);
    }
    audio_file.removeFromBottom(12);
    if (const MixScript::WaveAudioSource* track_playing = mixer->Playing()) {
        PaintAudioSource(g, audio_file.removeFromBottom(120), track_playing);
    }
    //audio_file_form.reduce(12, 12);
    //Rectangle<int> markers = audio_file_form.removeFromBottom(12);

    //g.setColour(Colour::fromRGB(0, 0, 0x88));
    //g.drawRect(audio_file_form);
    //audio_file_form.reduce(1,1);

    //g.setFont(10);
    //if (const MixScript::WaveAudioSource* track_playing = mixer->Playing()) {
    //    uint8_t* const audio_start = track_playing->audio_start;
    //    const float inv_duration = 1.f / (float)(track_playing->audio_end - audio_start);
    //    int cue_id = 1;
    //    for (const uint8_t* cue_pos : track_playing->cue_starts) {
    //        const float ratio = (cue_pos - audio_start) * inv_duration;
    //        const int pixel_pos = static_cast<int>(ratio * audio_file_form.getWidth()) + audio_file_form.getPosition().x;
    //        g.drawLine(pixel_pos, audio_file_form.getBottom(), pixel_pos, audio_file_form.getTopLeft().y, 1.f);
    //        g.drawText(juce::String::formatted("%i", cue_id),
    //            pixel_pos - 6,
    //            markers.getPosition().y + 1,
    //            12,
    //            10,
    //            juce::Justification::centred);
    //        ++cue_id;
    //    }
    //    g.setColour(Colour::fromRGB(0xFF, 0xFF, 0xFF));
    //    const float play_pos_ratio = track_playing->last_read_pos * inv_duration;
    //    const int play_pos = static_cast<int>(play_pos_ratio * audio_file_form.getWidth()) + audio_file_form.getPosition().x;
    //    g.drawLine(play_pos, audio_file_form.getBottom(), play_pos, audio_file_form.getTopLeft().y);
    //}
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

    juce::Rectangle<int> row_out = row_next();
    button_outfile.setBounds(row_out.removeFromLeft(36));
    label_outfile.setBounds(row_out.removeFromLeft(350));
}
