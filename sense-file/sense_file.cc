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

// sense-file: file-mode inference tutorial. Usage: sense-file <audio_file_path>
//
// Set kProjectKey below, then build. Shows the full SDK flow, config loading,
// and JSON result formatting in this one file.

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "sense/sense.hpp"

namespace {

// >>> Set your project key here before building. <<<
constexpr char kProjectKey[] = "YOUR_PROJECT_KEY";

// Reads a whole file into a string; returns "" if it cannot be opened.
std::string ReadFileToString(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) return {};
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
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
// JSON. An error frame is just reported.
class ResultPrinter : public sense::FrameResultListener {
 public:
  // Attach after Build() so OnResult can read the live feature state.
  void set_processor(const sense::AudioProcessor* processor) noexcept {
    processor_ = processor;
  }

  void OnResult(const sense::FrameResult& result) override {
    if (!sense::IsOk(result)) {
      std::cerr << "[error] " << result.error << "\n";
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

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: sense-file <audio_file_path>\n";
    return EXIT_FAILURE;
  }
  const std::string audio_path = argv[1];

  // Load settings (config.json next to the binary; missing is non-fatal).
  std::string config = ReadFileToString("config.json");
  if (config.empty())
    std::cerr << "[warn] could not read config.json; using default settings.\n";

  // Initialise the SDK with the project key and configuration.
  std::string init_err;
  if (!sense::Init(kProjectKey, std::move(config), &init_err)) {
    std::cerr << "sense::Init failed: " << init_err << "\n";
    return EXIT_FAILURE;
  }

  // Build a file-mode processor that delivers results to our callback.
  ResultPrinter listener;
  std::string build_err;
  std::unique_ptr<sense::AudioProcessor> processor =
      sense::AudioProcessorBuilder()
          .SetSourceType(sense::AudioSourceType::kFile)
          .SetFilePath(audio_path)
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

  // StartProcessing blocks until the whole file is processed.
  std::string start_err;
  if (!processor->StartProcessing(&start_err)) {
    std::cerr << "StartProcessing failed: " << start_err << "\n";
    sense::Terminate();
    return EXIT_FAILURE;
  }

  sense::Terminate();
  return EXIT_SUCCESS;
}
