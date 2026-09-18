// Copyright 2020-2026 Cochl.
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

// sense-stream: real-time microphone inference tutorial. Usage: sense-stream
// (Ctrl-C to stop).
//
// Set kProjectKey below, then build. Shows the full SDK flow, config loading,
// and JSON result formatting in this one file; the microphone plumbing is in
// microphone.cc.

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "microphone.hpp"
#include "sense/sense.hpp"

namespace {

// >>> Set your project key here before building. <<<
constexpr char kProjectKey[] = "YOUR_PROJECT_KEY";

// Audio capture spec fed to the SDK.
constexpr int kNumChannels = 1;

// Audio bit depth: the capture encoding handed to PushAudioChunk (the SDK then
// converts it to float32 internally).
//   SDK-supported formats:           kFloat32, kInt16, kInt32, kFloat64.
//   This tutorial's mic (miniaudio): kFloat32, kInt16, kInt32 only.
// kFloat64 IS a valid SDK format, but miniaudio (the tutorial's vendored
// microphone-capture library) has no 64-bit-float capture format, so selecting
// it here fails at startup with "Requested sample format cannot be captured by
// miniaudio." Use kFloat64 only for file / pre-recorded input, not live
// capture. Feed 24-bit audio as kInt32, left-justified (sample << 8).
constexpr sense::SampleFormat kFormat = sense::SampleFormat::kFloat32;

// Fallback hop (seconds); used only when config.json is unavailable, otherwise
// config.json's default_hopsize wins.
constexpr double kHopSize = 1.0;

// Microphone capture rate in Hz. 0 = capture at the model's rate (queried from
// the SDK) -- no resampling, the usual choice. Set a specific HIGHER rate (e.g.
// 44100 or 48000) to capture at a device-friendly rate and let the SDK
// downsample to the model rate before inference. Capture rates BELOW the model
// rate are NOT supported (upsampling cannot recover the missing high
// frequencies the model needs) -- such a value is forced up to the model rate
// with a warning.
constexpr int kCaptureSampleRate = 0;

// Reads a whole file into a string; returns "" if it cannot be opened.
std::string ReadFileToString(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) return {};
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// Extracts "default_hopsize" (seconds) from a config.json string; returns
// kHopSize if the key is absent or unparseable.
double ParseHopSize(const std::string& config) {
  const std::string key = "\"default_hopsize\"";
  const std::size_t k = config.find(key);
  if (k == std::string::npos) return kHopSize;  // absent key

  const std::size_t colon = config.find(':', k + key.size());
  if (colon == std::string::npos) return kHopSize;  // malformed JSON

  const char* begin = config.c_str() + colon + 1;
  char* end = nullptr;
  const double value = std::strtod(begin, &end);
  if (end == nullptr || end == begin) return kHopSize;  // malformed number

  if (value <= 0.0) return kHopSize;  // non-positive is malformed

  return value;
}

// Writes one inference window as a pretty-printed JSON object.
void PrintResult(std::ostream& os, const sense::FrameResult& result) {
  os << "{\n";
  os << "  \"start_time\": " << result.start_time << ",\n";
  os << "  \"end_time\": " << result.end_time << ",\n";
  os << "  \"prediction_time_ms\": " << result.prediction_time_ms << ",\n";
  os << "  \"tags\": [";
  for (std::size_t i = 0; i < result.tags.size(); ++i) {
    const sense::FrameResult::Tag& tag = result.tags[i];
    os << (i == 0 ? "\n" : ",\n");
    os << "    {\n";
    os << "      \"name\": \"" << tag.name << "\",\n";
    os << "      \"probability\": " << tag.probability << "\n";
    os << "    }";
  }
  os << (result.tags.empty() ? "]" : "\n  ]");
  os << "\n}\n";
}

// Result callback: fires once per inference window. Prints Result Summary lines
// when that feature is on (config.json or a runtime toggle), else per-window
// JSON. An error frame stops processing.
class ResultPrinter : public sense::FrameResultListener {
 public:
  // Attach after Build() so OnResult can read the live feature state.
  void set_processor(const sense::AudioProcessor* processor) noexcept {
    processor_ = processor;
  }

  void OnResult(const sense::FrameResult& result) override {
    if (!sense::IsOk(result)) {
      std::cerr << "[error] " << result.error << "\n";
      sense::StopProcessing();
      return;
    }
    if (processor_ != nullptr &&
        processor_->controls().IsResultSummaryEnabled()) {
      for (const auto& line : result.summaries) std::cout << line << "\n";
    } else {
      PrintResult(std::cout, result);
    }
  }

 private:
  const sense::AudioProcessor* processor_ = nullptr;
};

}  // namespace

int main() {
  // Load settings (config.json next to the binary; missing is non-fatal). The
  // hop size comes from config.json's default_hopsize; kHopSize is the fallback
  // when config.json is unavailable.
  std::string config = ReadFileToString("config.json");
  double hop_size = kHopSize;
  if (config.empty())
    std::cerr << "[warn] could not read config.json; using default settings.\n";
  else
    hop_size = ParseHopSize(config);

  // Initialise the SDK with the project key and configuration.
  std::string init_err;
  if (!sense::Init(kProjectKey, std::move(config), &init_err)) {
    std::cerr << "sense::Init failed: " << init_err << "\n";
    return EXIT_FAILURE;
  }

  // Build a stream-mode (recorder) processor delivering results to our
  // callback.
  ResultPrinter listener;
  std::string build_err;
  std::unique_ptr<sense::AudioProcessor> processor =
      sense::AudioProcessorBuilder()
          .SetSourceType(sense::AudioSourceType::kRecorder)
          .SetCallback(&listener)
          .Build(&build_err);
  if (!processor) {
    std::cerr << "AudioProcessorBuilder::Build failed: " << build_err << "\n";
    sense::Terminate();
    return EXIT_FAILURE;
  }
  listener.set_processor(processor.get());

  // Optional: drive features at runtime via processor->controls() (safe after
  // Build(); AAD/AGC are stream-mode only):
  //   sense::Controls& controls = processor->controls();
  //   controls.SetSensitivity("HIGH");  // VERY_LOW|LOW|NORMAL|HIGH|VERY_HIGH
  //   controls.SetTagSensitivity("Footstep", "LOW");  // per-tag override
  //   controls.SetResultSummaryEnabled(true);
  //   controls.EnableAudioActivityDetection(true);  // stream-mode only
  //   controls.EnableAutomaticGainControl(true);    // stream-mode only

  std::string start_err;
  if (!processor->StartProcessing(&start_err)) {
    std::cerr << "StartProcessing failed: " << start_err << "\n";
    sense::Terminate();
    return EXIT_FAILURE;
  }

  // Capture the mic until Ctrl-C. RunMicrophone (microphone.cc) feeds each
  // chunk to the SDK via the public sense::AudioProcessor::PushAudioChunk() --
  // the one SDK call that drives stream inference.
  // Resolve the capture rate: kCaptureSampleRate (0 -> the model's rate), then
  // clamp up to the model rate (its floor). Higher rates are resampled down to
  // the model rate by the SDK inside PushAudioChunk.
  const int model_rate = processor->model_sample_rate();
  int sample_rate = kCaptureSampleRate > 0 ? kCaptureSampleRate : model_rate;
  if (sample_rate < model_rate) {
    std::cerr << "[warn] capture rate " << sample_rate << " Hz is below the "
              << "model rate " << model_rate << " Hz; capture rates below the "
              << "model rate are not supported (upsampling cannot recover the "
              << "missing high frequencies the model needs). Using the model "
              << "rate " << model_rate << " Hz instead.\n";
    sample_rate = model_rate;
  }
  const bool ok = tutorial::RunMicrophone(*processor, kNumChannels, kFormat,
                                          hop_size, sample_rate);
  processor->StopProcessing();
  sense::Terminate();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
