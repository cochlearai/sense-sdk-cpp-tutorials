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

#include "AudioFile.h"

#include "sense/sense.hpp"
#include "sense/audio_source_file.hpp"

AudioFile<double> wav_file;

// The recommended sample rate is 22050.
// Changing this value will impact the performances.
#define SAMPLE_RATE (22050)

// For the file prediction, the sense accepts any kind of audio sample size
// the sample size will then be cut in different frames depending on
// the SAMPLE_RATE provided during the sense.Init (default is 22050).
// Use the AudioSourceFile if you want to process a large content at once.
void FilePrediction() {
  sense::AudioSourceFile<double> audio_source_file;

  // Returns a Result object containing multiple FrameResult.
  sense::Result result = audio_source_file.Predict(wav_file.samples[0]);
  std::cout << result << std::endl;
}

int main(int argc, char *argv[]) {
  // Read a .wav file.
  if (argc != 2) {
    std::cout << "Usage: sense-file <.wav file name>" << std::endl;
    exit(0);
  }
  wav_file.load(argv[1]);

  // Quick shortcut to print a summary to the console.
  wav_file.printSummary();

  sense::Parameters sense_params;
  sense_params.audio_format = sense::AF_DOUBLE;

  if (sense::Init("Your project key", sense_params) < 0) {
    return -1;
  }

  FilePrediction();
  return 0;
}
