# sense-sdk-cpp-tutorials

- `sense-file/` - file-mode: load a local audio file and run inference to completion (`sense-file-app <audio_file>`).
- `sense-stream/` - stream-mode: capture the microphone and infer in real time (`sense-stream-app`).

## 1. Build

Prerequisites: a C++14 compiler, CMake >= 3.10, and the Cochl.Sense Edge SDK staged at `./sense` (`include/` + `lib/`).

```bash
cmake -B build
cmake --build build
```

The binaries `sense-file-app` and `sense-stream-app` land in this directory, next to `config.json` and `audio-files/`, so you can run them in place.

## 2. Run

Set your project key in `kProjectKey` (top of `sense_file.cc` / `sense_stream.cc`). A `config.json` in the working directory is used if present (missing is non-fatal).

```bash
./sense-file-app audio-files/babycry.wav     # file mode (blocks until end of file)
./sense-stream-app                           # mic mode (Ctrl-C to stop)
```

Each prints one JSON result per inference window (`start_time`, `end_time`, `prediction_time_ms`, `tags`); a `summaries` field is added when Result Summary is enabled.

## 3. Notes

- **`config.json`** (optional) controls sensitivity, Result Summary, Audio Activity Detection, and Automatic Gain Control (AAD/AGC are stream-only); the same knobs are available at runtime via `processor->controls()` after `Build()`.
- **Callback:** each tutorial's `ResultPrinter` (a `sense::FrameResultListener`) fires `OnResult` once per window on the SDK's inference thread -- edit it or pass your own to `.SetCallback()`.
- **Capture spec (stream)**, at the top of `sense_stream.cc`: `kNumChannels`, `kFormat`, `kCaptureSampleRate` (`0` = model rate; higher is downsampled), `kHopSize`. `kFormat` is `sense::SampleFormat::{kFloat32,kInt16,kInt32}`; feed 24-bit audio as `kInt32`, left-justified (`sample << 8`). `sense-file` reads its format from the file header. Microphone capture is tutorial-side (vendored miniaudio); the SDK has no audio-device dependency.
