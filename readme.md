
# DtBlkFx (macOS Universal Edition)<img width="1280" height="720" alt="dtblk" src="https://github.com/user-attachments/assets/a1c9232e-b093-4b48-aa10-92aa534abed4" />


**DtBlkFx** is a Fast Fourier Transform (FFT) based VST plug-in for macOS, originally created by Darrell Tam and updated for modern macOS (Universal Binary) and JUCE by haiori.

It works by processing audio in the frequency domain, allowing for unique effects like:
- **Spectral Vocoding**
- **Harmonic Filtering**
- **Frequency Shifting**
- **Spectral Smearing**

## Features
- **Universal Support**: Native compatibility for both Apple Silicon (M1/M2/M3) and Intel processors.
- **Modern GUI**: Rebuilt user interface using the JUCE framework.
- **Stereo Processing**: True stereo operation for all effects.
- **Ad-hoc Signed**: Ready for local development and use in DAWs like Ableton Live.

## Installation

1.  **Download/Build**: Ensure you have the `DtBlkFx_GUI.vst3` bundle.
2.  **Install**: Copy the plugin to your VST3 directory:
    ```bash
    cp -R build/universal/DtBlkFx_GUI.vst3 ~/Library/Audio/Plug-Ins/VST3/
    ```
3.  **Rescan**: Open your DAW (e.g., Ableton Live) and rescan for plugins.

## Building from Source

### Prerequisites
- **Xcode** or the Xcode Command Line Tools (`xcode-select --install`)
- **CMake** 3.22 or newer (`brew install cmake`); CMake 4.x works
- Optional: **Ninja** (`brew install ninja`) for faster builds
- An internet connection on the first configure: FFTW 3.3.10 is downloaded from fftw.org and built
  automatically for every architecture you target

JUCE (9.0.4) is a git submodule. vcpkg is no longer needed.

### Quick build (script)

```bash
git clone --recursive <repo-url> dtblkfx_mac
cd dtblkfx_mac
./build_universal.sh            # Universal (Apple Silicon + Intel)
./build_universal.sh arm64      # Apple Silicon only (faster)
./build_universal.sh install    # Universal, then copy into ~/Library/Audio/Plug-Ins/VST3
```

### Plain CMake

```bash
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release                 # Universal by default on macOS
cmake --build build --config Release --parallel
```

The plug-in ends up at `build/DtBlkFx_artefacts/Release/VST3/DtBlkFx_GUI.vst3`.

Useful options:

| Option | Default | Meaning |
| --- | --- | --- |
| `CMAKE_OSX_ARCHITECTURES` | `arm64;x86_64` | Use `arm64` for an Apple Silicon only build |
| `CMAKE_OSX_DEPLOYMENT_TARGET` | `10.13` | Oldest macOS the plug-in runs on |
| `DTBLKFX_USE_SYSTEM_FFTW` | `OFF` | Use an installed FFTW via pkg-config (e.g. `brew install fftw pkg-config`). Homebrew's FFTW is arm64-only, so pair it with `-DCMAKE_OSX_ARCHITECTURES=arm64` |
| `DTBLKFX_COPY_PLUGIN_AFTER_BUILD` | `OFF` | Copy the VST3 to `~/Library/Audio/Plug-Ins/VST3` after each build |

Xcode project: `cmake -B build-xcode -G Xcode` and open the generated project.

The same CMake setup also builds on Linux (VST3 `.so` bundle) and should work on Windows.

## Usage
- **Mix Back**: Controls the balance between the original and processed signal.
- **Delay**: Adds a delay to the processed signal.
- **FFT Length**: Adjusts the size of the FFT window (frequency resolution vs. time resolution).
- **Overlap**: Controls the overlap of FFT windows (smoother sound vs. CPU usage).
- **Effect Parameters**:
    - **Freq A/B**: Frequency range for the effect.
    - **Amp**: Amplitude of the effect.
    - **Val**: Effect-specific parameter.

## License
This project is licensed under the GNU General Public License v2.0 (or later). See `COPYING` for details.

## Credits
- **Original Author**: Darrell Tam
- **macOS/JUCE Port**: haiori
