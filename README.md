# QFlip — seamless reverse sampler

Built with Qondabeat. Drag a sample in, flip it, pitch it, drag the result
straight back out to your DAW.

## MVP (this scaffold)

- **Drag & drop / browse** to load WAV, MP3, AIFF, FLAC, OGG
- **Reverse** toggle (on by default) — the whole point
- **Pitch** −24..+24 semitones with smooth interpolated playback
- **Gain** 0..2
- **Drag-out pad** — renders the processed sample to a WAV and lets you
  drag it directly onto a DAW track (the killer feature)
- Free-running loop auditioning, VST3 / AU / Standalone

## V2 roadmap

- Chop mode (slice into N pieces, rearrange / MIDI trigger)
- Tempo-synced gate
- Host beat-sync (RubberBand time-stretch)

## Build

Requires: C++17 compiler, CMake 3.22+, git. First build downloads JUCE
(~150MB) and takes a while — that's normal.

**Linux:**
```bash
sudo apt install build-essential cmake git \
  libasound2-dev libjack-jackd2-dev ladspa-sdk \
  libcurl4-openssl-dev libfreetype6-dev libx11-dev libxcomposite-dev \
  libxcursor-dev libxcursor-dev libxrandr-dev libxinerama-dev \
  libgl1-mesa-dev libglu1-mesa-dev

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# VST3 lands in: build/QFlip_artefacts/Release/VST3/
```

**macOS:**
```bash
brew install cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build -j
# VST3 + AU land in: build/QFlip_artefacts/Release/
```

**Windows:** Visual Studio 2022 + CMake, same two commands.

## Project layout

```
Source/
  PluginProcessor.h/.cpp  — audio engine: reverse playback, pitch, render/export
  PluginEditor.h/.cpp      — UI: waveform drop zone, knobs, drag-out pad
```

## How the engine works (for learning)

- The dropped file is decoded once into `sampleBuffer` (an in-memory
  `AudioSampleBuffer`).
- `processBlock` walks a fractional `playhead` through the buffer. Negative
  step = reversed. Pitch knob sets the step size via `2^(semitones/12)`.
- `readSampleLinear` does linear interpolation between adjacent samples so
  pitched playback doesn't get crunchy.
- `renderProcessed()` re-walks the buffer offline with the current settings
  and returns a fresh buffer — that's what the drag-out pad writes to WAV.
