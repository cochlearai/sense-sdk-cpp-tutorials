// Copyright 2026 Cochl.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.

// microphone.hpp: microphone capture for the stream tutorial. All of the
// device, threading, and Ctrl-C handling lives in microphone.cc so the tutorial
// itself shows only SDK calls. The sense SDK has no audio-device dependency;
// capture is entirely tutorial-side (via the vendored miniaudio header).

#ifndef SENSE_TUTORIAL_MICROPHONE_HPP_
#define SENSE_TUTORIAL_MICROPHONE_HPP_

#include "sense/sense.hpp"

namespace tutorial {

/**
 * @brief Captures the default microphone into @p processor until Ctrl-C.
 *
 * Captures at @p sample_rate with @p num_channels, then feeds PushAudioChunk()
 * one @p hop_size window (seconds) at a time -- the drain loop dequeues in
 * hop-sized steps -- until SIGINT (Ctrl-C) requests a cooperative stop. The
 * capture rate is passed through to PushAudioChunk so the SDK resamples to the
 * model rate when they differ (and downmixes multi-channel input to mono).
 * Owns the capture device, the producer/consumer buffer, and the signal handler
 * internally.
 *
 * @param processor     A started stream-mode (recorder) processor.
 * @param num_channels  Capture channel count (1 = mono).
 * @param format        Sample encoding to capture in; this tutorial uses
 *                      kFloat32/kInt16/kInt32 and returns false for any other
 *                      (the 1-byte formats and kFloat64 are omitted here,
 *                      though the SDK/resampler still support them).
 * @param hop_size      Window pushed per drain step, in seconds (e.g. 1.0);
 *                      returns false if not positive.
 * @param sample_rate   Capture rate in Hz; must be positive and at least the
 *                      model rate (see AudioProcessor::model_sample_rate()).
 * @return true on a clean Ctrl-C stop; false on a device or push failure.
 *
 * @side_effects VIOLATION: Non-referentially transparent
 * @side_effects_reason Opens a hardware capture device and installs a signal
 *   handler; drives inference from live audio.
 * @side_effects_what Microphone I/O, a SIGINT handler, a capture thread.
 * @side_effects_impact Blocks the caller until Ctrl-C.
 * @side_effects_alternatives None -- live capture requires device + signal I/O.
 */
bool RunMicrophone(sense::AudioProcessor& processor, int num_channels,
                   sense::SampleFormat format, double hop_size,
                   int sample_rate);

}  // namespace tutorial

#endif  // SENSE_TUTORIAL_MICROPHONE_HPP_
