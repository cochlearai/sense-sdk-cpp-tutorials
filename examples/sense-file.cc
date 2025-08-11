// Copyright 2020-2024 Cochl.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>

#include "sense/audio_source_file.hpp"
#include "sense/sense.hpp"

// The file with a sample rate lower than 22,050 Hz can’t be used.
// If the sample rate is higher than 22,050 Hz, The Sense SDK will
// downsample the audio internally.
bool FilePrediction(const std::string& file_path) {
  // Create a sense audio file instance
  sense::AudioSourceFile audio_source_file;
  const bool result_summary =
      sense::get_parameters().result_summary.enable;

  if (audio_source_file.Load(file_path) < 0) return false;

  // Run the prediction, and it will return a 'Result' object containing
  // multiple 'FrameResult' objects.
  sense::Result result = audio_source_file.Predict();
  if (!result) {
    std::cerr << result.error << std::endl;
    return false;
  }

  if (result_summary) {
    std::cout << "<Result summary>" << std::endl;
    for (const auto& summary : result.summaries)
      std::cout << summary << std::endl;
    // Even if you use the result summary, you can still get precise
    // results like below if necessary:
    // std::cout << result << std::endl;
  } else {
    std::cout << result << std::endl;
  }
  return true;
}

int main(int argc, char* argv[]) {
  // Read a .wav file.
  if (argc != 2) {
    std::cout << "Usage: sense-file <PATH_TO_AUDIO_FILE>" << std::endl;
    return 0;
  }

  std::string config_file_path = "./config.json";
  std::string project_key = "Your project key";
  if (sense::Init(project_key, config_file_path) < 0) return -1;

  std::vector<std::string> selected_tags = sense::get_selected_tags();
  std::cout << "Selected tags: " << std::endl;;
  for (const auto& tag : selected_tags) {
    std::cout << "** "  << tag << std::endl;
  }
  std::cout << "--------------------------------" << std::endl;
  std::cout << std::endl;

  if (!FilePrediction(argv[1]))
    std::cerr << "File prediction failed." << std::endl;
  sense::Terminate();
  return 0;
}
