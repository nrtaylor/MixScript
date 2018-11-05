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
    track_playing(nullptr),
    track_incoming(nullptr),
    queued_cue(0)
{
    // TODO: MenuBarModel

    // Make sure you set the size of the component after
    // you add any child components.
    setSize (1024, 600);

    addAndMakeVisible(menuBar);
    
    //track_playing = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(
    //    "C:\\Programming\\MixScript\\mix_script_test_file_seed.wav")));

    //track_incoming = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(
    //    "C:\\Programming\\MixScript\\mix_script_test_file_juju.wav")));

    track_playing = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(
        "C:\\Programming\\MixScript\\mix_script_test_file_juju_outro.wav")));

    track_incoming = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(
        "C:\\Programming\\MixScript\\mix_script_test_file_martsman.wav")));
    //MixScript::WriteWaveFile("C:\\Programming\\MixScript\\Output\\mix_script_test_file_juju.wav", track_incoming);
    //MixScript::WaveAudioSource* audio_source = MixScript::LoadWaveFile(
    //    "C:\\Programming\\MixScript\\one_secondno.wav");

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

    if (playback_paused.load()) {
        return;
    }

    int32_t cue_pos = queued_cue.load();
    if (cue_pos != 0) {
        queued_cue.compare_exchange_strong(cue_pos, 0); // TODO: don't block on audio thread
        if (cue_pos != 0) {
            MixScript::ResetToCue(track_playing, static_cast<uint32_t>(cue_pos > 0 ? cue_pos : 0));
        }
    }

    MixScript::FloatOutputWriter output_writer = { bufferToFill.buffer->getWritePointer(0),
        bufferToFill.buffer->getWritePointer(1) };

    MixScript::Mixer mixer;
    mixer.modifier_mono = modifier_mono.load(); // TODO: make mixer member
    mixer.Mix(track_playing, track_incoming, output_writer, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

enum TrackKeys : int {
    Track_Home = 65572,
};

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
        MixScript::Mixer mixer;
        mixer.Save(file.getFullPathName().toRawUTF8(), track_playing, track_incoming);
    }
    playback_paused = paused_state;
}

void MainComponent::ExportRender() {
    const bool paused_state = playback_paused.load();
    playback_paused = true;
    FileChooser chooser("Select Output File", juce::File::getCurrentWorkingDirectory(), "*.wav");
    if (chooser.browseForFileToOpen()) {
        std::unique_ptr<MixScript::WaveAudioSource> output_source = std::unique_ptr<MixScript::WaveAudioSource>(
            std::move(MixScript::Render(track_playing, track_incoming)));
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
        menu.addItem(2, "Export");
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
        ExportRender();
        break;
    default:
        break;
    }
}

void MainComponent::timerCallback() {
    repaint();
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
    const Rectangle<int> bounds = g.getClipBounds();
    Rectangle<int> audio_file = bounds;
    Rectangle<int> audio_file_form = audio_file.removeFromBottom(120);
    audio_file_form.reduce(12, 12);
    Rectangle<int> markers = audio_file_form.removeFromBottom(12);

    g.setColour(Colour::fromRGB(0, 0, 0x88));
    g.drawRect(audio_file_form);
    audio_file_form.reduce(1,1);

    g.setFont(10);
    if (track_playing) {
        uint8_t* const audio_start = track_playing->audio_start;
        const float inv_duration = 1.f / (float)(track_playing->audio_end - audio_start);
        int cue_id = 1;
        for (const uint8_t* cue_pos : track_playing->cue_starts) {
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
        const float play_pos_ratio = track_playing->last_read_pos * inv_duration;
        const int play_pos = static_cast<int>(play_pos_ratio * audio_file_form.getWidth()) + audio_file_form.getPosition().x;
        g.drawLine(play_pos, audio_file_form.getBottom(), play_pos, audio_file_form.getTopLeft().y);
    }
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
    menuBar.setBounds(0, 0, getWidth(), 24);
}
