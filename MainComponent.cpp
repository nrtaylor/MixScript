/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"
#include "WavAudioSource.h"

//==============================================================================
MainComponent::MainComponent() :
    track_playing(nullptr),
    track_incoming(nullptr),
    queued_cue(0)
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (800, 600);
    
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

    //const int channels = bufferToFill.buffer->getNumChannels();
    //int samples_remaining = bufferToFill.numSamples;
    //int sample_offset = bufferToFill.startSample;

    uint32_t cue_pos = queued_cue.load();
    if (cue_pos != 0) {
        queued_cue.compare_exchange_strong(cue_pos, 0); // TODO: don't block on audio thread
        if (cue_pos != 0) {
            MixScript::ResetToCue(track_playing, cue_pos);
        }
    }

    MixScript::FloatOutputWriter output_writer = { bufferToFill.buffer->getWritePointer(0),
        bufferToFill.buffer->getWritePointer(1) };

    MixScript::Mixer mixer;
    mixer.Mix(track_playing, track_incoming, output_writer, bufferToFill.numSamples);
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
    if (key_code == (int)'R') {
        ExportRender();
    }
    if (key_code == (int)' ') {
        const bool set_playback_paused = !playback_paused.load();
        playback_paused = set_playback_paused;
    }
    return false;
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

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
}
