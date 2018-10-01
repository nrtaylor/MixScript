// WaveAudioSource - reads wav file into memory
// Author - Nic Taylor

#pragma once

namespace MixScript
{
    struct WaveAudioSource;
    WaveAudioSource* LoadWaveFile(const char* file_path);
}
