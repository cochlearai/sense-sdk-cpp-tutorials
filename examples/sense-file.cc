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

// For the file prediction, the sense accepts any kind of audio sample size
// the sample size will then be cut in different frames depending on
// the SAMPLE_RATE provided during the sense.Init (default is 22050).
// Use the AudioSourceFile if you want to process a large content at once.
bool FilePrediction(const std::string& file_path) {
  sense::AudioSourceFile audio_source_file;
  if (audio_source_file.Load(file_path) < 0)
    return false;

  // Returns a Result object containing multiple FrameResult.
  sense::Result result = audio_source_file.Predict();
  std::cout << result << std::endl;
  return true;
}

int main(int argc, char *argv[]) {
  // Read a .wav file.
  if (argc != 2) {
    std::cout << "Usage: sense-file <.wav file name>" << std::endl;
    exit(0);
  }

  sense::Parameters sense_params;
  sense_params.metrics.retention_period = 0;   // range, 1 to 31 days
  sense_params.metrics.free_disk_space = 100;  // range, 0 to 1,000,000 MB
  sense_params.metrics.push_period = 30;       // range, 1 to 3,600 seconds
  sense_params.log_level = 0;

  sense_params.device_name = "Testing device";

  if (sense::Init("Your project key",
                  sense_params) < 0) {
    return -1;
  }

  if (!FilePrediction(argv[1]))
    std::cerr << "File prediction failed." << std::endl;
  sense::Terminate();
  return 0;
}
