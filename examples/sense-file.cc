/*
  Copyright 2020 Thibault Bougerolles <tbougerolles@cochlear.ai>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <iostream>

#include "sense/sense.hpp"
#include "sense/audio_source_file.hpp"

// The file with a sample rate lower than 22,050 Hz can’t be used.
// If the sample rate is higher than 22,050 Hz, The Sense SDK will
// downsample the audio internally.
bool FilePrediction(const std::string& file_path) {
  // Create a sense audio file instance
  sense::AudioSourceFile audio_source_file;
  const bool result_abbreviation =
      sense::get_parameters().result_abbreviation.enable;

  if (audio_source_file.Load(file_path) < 0) return false;

  // Run the prediction, and it will return a 'Result' object containing
  // multiple 'FrameResult' objects.
  sense::Result result = audio_source_file.Predict();
  if (!result) {
    std::cerr << result.error << std::endl;
    return false;
  }
  if (result_abbreviation) {
    std::cout << "<Result summary>" << std::endl;
    for (const auto& abbreviation : result.abbreviations)
      std::cout << abbreviation << std::endl;
    // Even if you use the result abbreviation, you can still get precise
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

  sense::Parameters sense_params;
  sense_params.metrics.retention_period = 0;   // range, 1 to 31 days
  sense_params.metrics.free_disk_space = 100;  // range, 0 to 1,000,000 MB
  sense_params.metrics.push_period = 30;       // range, 1 to 3,600 seconds
  sense_params.log_level = 0;

  sense_params.device_name = "Testing device";

  sense_params.hop_size_control.enable = true;
  sense_params.sensitivity_control.enable = true;
  sense_params.result_abbreviation.enable = true;
  sense_params.label_hiding.enable = false;  // stream mode only

  if (sense::Init("Your project key",
                  sense_params) < 0) {
    return -1;
  }

  if (!FilePrediction(argv[1]))
    std::cerr << "File prediction failed." << std::endl;
  sense::Terminate();
  return 0;
}

