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
    track_incoming(nullptr)    
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (800, 600);
    
    track_playing = std::unique_ptr<MixScript::WaveAudioSource>(std::move(MixScript::LoadWaveFile(
        "C:\\Programming\\MixScript\\mix_script_test_file_juju.wav")));

    //MixScript::WaveAudioSource* audio_source = MixScript::LoadWaveFile(
    //    "C:\\Programming\\MixScript\\one_secondno.wav");

    // specify the number of input and output channels that we want to open
    setAudioChannels (0, 2);
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

    //const int channels = bufferToFill.buffer->getNumChannels();
    //int samples_remaining = bufferToFill.numSamples;
    //int sample_offset = bufferToFill.startSample;

    MixScript::ReadSamples(track_playing, bufferToFill.buffer->getWritePointer(0), bufferToFill.buffer->getWritePointer(1),
        bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
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
