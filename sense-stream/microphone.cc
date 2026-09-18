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

// microphone.cc: live capture backing tutorial::RunMicrophone().
//
// miniaudio delivers microphone frames on its own real-time audio thread, so
// the capture callback only appends raw PCM to a mutex-guarded buffer; the
// caller's thread drains that buffer and calls PushAudioChunk() (which runs
// inference synchronously when a window is ready). Ctrl-C sets an atomic flag
// that ends the drain loop cleanly.

#include "microphone.hpp"

#include <csignal>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "miniaudio.h"

namespace tutorial {
namespace {

// Poll interval for the drain loop while less than one hop is buffered. The
// channel count, format, hop size, and capture rate all come from the caller
// (see sense_stream.cc) -- the rate is the model's sample rate, queried from
// the processor, not hardcoded here.
constexpr int kIdlePollMs = 10;

// Set by the SIGINT handler; ends the capture/drain loop cooperatively.
std::atomic<bool> g_stop{false};

// Maps an SDK SampleFormat to the miniaudio capture format. Returns
// ma_format_unknown for formats this tutorial omits (kFloat64); the
// SDK/resampler still support it.
ma_format ToMaFormat(sense::SampleFormat format) {
  switch (format) {
    case sense::SampleFormat::kFloat32:
      return ma_format_f32;
    case sense::SampleFormat::kInt16:
      return ma_format_s16;
    case sense::SampleFormat::kInt32:
      return ma_format_s32;
    case sense::SampleFormat::kFloat64:
    default:
      return ma_format_unknown;
  }
}

// Human-readable name for an SDK SampleFormat (for diagnostics).
const char* FormatName(sense::SampleFormat format) {
  switch (format) {
    case sense::SampleFormat::kFloat32:
      return "kFloat32";
    case sense::SampleFormat::kInt16:
      return "kInt16";
    case sense::SampleFormat::kInt32:
      return "kInt32";
    case sense::SampleFormat::kFloat64:
      return "kFloat64";
    default:
      return "unknown";
  }
}

// Shared between the audio thread (producer) and the caller (consumer).
struct CaptureBuffer {
  std::mutex mutex;
  std::vector<uint8_t> bytes;
  int num_channels = 1;
  int bytes_per_frame = static_cast<int>(sizeof(float));
  sense::SampleFormat format = sense::SampleFormat::kFloat32;
};

void HandleSigint(int /*signum*/) { g_stop.store(true); }

// miniaudio callback: append captured PCM. Runs on the real-time audio thread,
// so it does the minimum -- copy under the mutex -- and never touches the SDK.
void CaptureCallback(ma_device* device, void* /*output*/, const void* input,
                     ma_uint32 frame_count) {
  auto* capture = static_cast<CaptureBuffer*>(device->pUserData);
  if (capture == nullptr || input == nullptr) return;
  const auto* bytes = static_cast<const uint8_t*>(input);
  const std::size_t byte_count =
      static_cast<std::size_t>(frame_count) *
      static_cast<std::size_t>(capture->bytes_per_frame);
  std::lock_guard<std::mutex> lock(capture->mutex);
  capture->bytes.insert(capture->bytes.end(), bytes, bytes + byte_count);
}

// Drains captured PCM into the SDK one hop-sized window at a time, until Ctrl-C
// or a rejected push. Accumulates captured bytes in a pending buffer and
// dequeues each full @p hop_bytes window; any remainder carries to the next
// round.
bool DrainLoop(sense::AudioProcessor& processor, CaptureBuffer& capture,
               int hop_bytes, int sample_rate) {
  std::vector<uint8_t> pending;
  while (!g_stop.load()) {
    {
      std::lock_guard<std::mutex> lock(capture.mutex);
      pending.insert(pending.end(), capture.bytes.begin(), capture.bytes.end());
      capture.bytes.clear();
    }
    std::size_t offset = 0;
    while (pending.size() - offset >= static_cast<std::size_t>(hop_bytes)) {
      if (!processor.PushAudioChunk(pending.data() + offset, hop_bytes,
                                    capture.num_channels, capture.format,
                                    sample_rate)) {
        std::cerr << "PushAudioChunk rejected a chunk.\n";
        return false;
      }
      offset += static_cast<std::size_t>(hop_bytes);
    }
    if (offset > 0)
      pending.erase(pending.begin(),
                    pending.begin() + static_cast<std::ptrdiff_t>(offset));
    std::this_thread::sleep_for(std::chrono::milliseconds(kIdlePollMs));
  }
  return true;
}

}  // namespace

bool RunMicrophone(sense::AudioProcessor& processor, int num_channels,
                   sense::SampleFormat format, double hop_size,
                   int sample_rate) {
  const ma_format ma_fmt = ToMaFormat(format);
  if (ma_fmt == ma_format_unknown) {
    std::cerr << "Requested capture format " << FormatName(format)
              << " is not supported by miniaudio (it has no 64-bit-float "
                 "capture format). Use kFloat32, kInt16, or kInt32 for live "
                 "mic capture; kFloat64 is valid only for file input. This is "
                 "unrelated to the capture rate.\n";
    return false;
  }
  if (hop_size <= 0.0) {
    std::cerr << "hop_size must be positive.\n";
    return false;
  }
  if (sample_rate <= 0) {
    std::cerr << "sample_rate must be positive.\n";
    return false;
  }
  CaptureBuffer capture;
  capture.num_channels = num_channels;
  capture.format = format;
  capture.bytes_per_frame =
      static_cast<int>(ma_get_bytes_per_sample(ma_fmt)) * num_channels;
  // One hop = hop_size seconds of audio at the capture rate.
  const int hop_bytes =
      static_cast<int>(hop_size * sample_rate) * capture.bytes_per_frame;

  ma_device_config device_config =
      ma_device_config_init(ma_device_type_capture);
  device_config.capture.format = ma_fmt;
  device_config.capture.channels = static_cast<ma_uint32>(num_channels);
  device_config.sampleRate = static_cast<ma_uint32>(sample_rate);
  device_config.dataCallback = CaptureCallback;
  device_config.pUserData = &capture;

  ma_device device;
  if (ma_device_init(nullptr, &device_config, &device) != MA_SUCCESS) {
    std::cerr << "Failed to open the default microphone.\n";
    return false;
  }

  std::signal(SIGINT, HandleSigint);
  bool ok = true;
  if (ma_device_start(&device) != MA_SUCCESS) {
    std::cerr << "Failed to start microphone capture.\n";
    ok = false;
  } else {
    std::cerr << "Listening on the default microphone. Press Ctrl-C to stop.\n";
    ok = DrainLoop(processor, capture, hop_bytes, sample_rate);
  }

  ma_device_uninit(&device);
  return ok;
}

}  // namespace tutorial
