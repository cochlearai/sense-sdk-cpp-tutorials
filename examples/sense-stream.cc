/*
  Copyright 2021 Thibault Bougerolles <tbougerolles@cochlear.ai>

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

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <signal.h>

#include <iostream>

#include "sense/sense.hpp"
#include "sense/audio_source_stream.hpp"

#define SAMPLE_RATE 22050
#define BUF_SIZE (SAMPLE_RATE) / 2

static bool running = true;

// Signals handling
static void handle_sigterm(int signo) {
  running = false;
}

void init_signal() {
  struct sigaction sa;
  sa.sa_flags = 0;

  sigemptyset(&sa.sa_mask);
  sa.sa_handler = handle_sigterm;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
}

// To run this example, please install pulseaudio on your machine using the
// following command:
//
// $ sudo apt install -y libpulse-dev
//
// Please ensure that pulseaudio is configured with a valid input by running
// the following command:
//
// $ pacmd list-sources | grep -e 'index:' -e device.string -e 'name:'
//
// To set the default source to the desired source, run the following command:
//
// $ pacmd set-default-source <DEVICE_INDEX>
//
// It's also quite common for pulseaudio to encounter startup issues.
//
// $ pulseaudio -k # Terminate the process if necessary
// $ pulseaudio -D # Restart it
bool StreamPrediction() {
  // Create the recording stream
  static pa_sample_spec ss;
  ss.format = PA_SAMPLE_S16LE;  // May vary based on your system (int16_t)
  ss.rate = SAMPLE_RATE;
  ss.channels = 1;
  pa_simple* s = NULL;
  int error = 0;
  if (!(s = pa_simple_new(NULL, "sense-stream",
                          PA_STREAM_RECORD, NULL,
                          "record", &ss,
                          NULL, NULL, &error))) {
    fprintf(stderr,
            __FILE__": pa_simple_new() failed: %s\n",
            pa_strerror(error));
    return false;
  }

  // Create a sense audio stream instance
  sense::AudioSourceStream audio_source_stream;
  std::vector<int16_t> audio_sample;
  const bool result_abbreviation =
      sense::get_parameters().result_abbreviation.enable;
  const bool hop_size_control =
      sense::get_parameters().hop_size_control.enable;

  // The Sense SDK is meant to be used with the audio frames overlapping:
  //
  //   [+ + + +]             : first frame,  0.0-1.0 s
  //       [+ + + +]         : second frame, 0.5-1.5 s
  //       ^   [+ + + +]     : third frame,  1.0-2.0 s
  //       |       [+ + + +] : fourth frame, 1.5-2.5 s
  //       |                ...
  //       0.5 seconds later
  //
  // This is why our buffer length is set with SAMPLE_RATE / 2.
  // Every iteration we will pop 0.5 s of audio from the front,
  // and push back 0.5 s.
  // The main reason to do this is to ensure we catch an event if something
  // occurs between two frames.
  for (bool half_second = false; running; half_second = !half_second) {
    // Record some data ...
    int16_t buf[BUF_SIZE];
    if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
      fprintf(stderr,
              __FILE__": pa_simple_read() failed: %s\n",
              pa_strerror(error));
      break;
    }

    if (audio_sample.empty()) {
      audio_sample = {buf, buf + BUF_SIZE};
      audio_sample.insert(audio_sample.begin() + BUF_SIZE,
                          std::begin(buf), std::end(buf));
      continue;
    }
    audio_sample.erase(audio_sample.begin(),
                       audio_sample.begin() + BUF_SIZE);
    audio_sample.insert(audio_sample.begin() + BUF_SIZE,
                        std::begin(buf), std::end(buf));

    // If the hop size control is not enabled, then the predictions must be
    // made at intervals of 1 second.
    // In other words, we will disregard any data from intervals that start at
    // 0.5-second frames.
    if (!hop_size_control && half_second) continue;

    // Run the prediction, and it will return a 'FrameResult' object.
    sense::FrameResult frame_result = audio_source_stream.Predict(audio_sample);
    if (!frame_result) {
      std::cerr << frame_result.error << std::endl;
      break;
    }
    if (result_abbreviation) {
      for (const auto& abbreviation : frame_result.abbreviations)
        std::cout << abbreviation << std::endl;
      // Even if you use the result abberviation, you can still get precise
      // results like below if necessary:
      // std::cout << frame_result << std::endl;
    } else {
      std::cout << "---------NEW FRAME---------" << std::endl;
      std::cout << frame_result << std::endl;
    }
  }
  if (s) pa_simple_free(s);
  // If the while statement is exited due to an exception even if the running is
  // true, then return false.
  return running ? false : true;
}

int main(int argc, char* argv[]) {
  init_signal();

  sense::Parameters sense_params;
  sense_params.metrics.retention_period = 0;   // range, 1 to 31 days
  sense_params.metrics.free_disk_space = 100;  // range, 0 to 1,000,000 MB
  sense_params.metrics.push_period = 30;       // range, 1 to 3,600 seconds
  sense_params.log_level = 0;

  sense_params.device_name = "Testing device";

  sense_params.hop_size_control.enable = true;
  sense_params.sensitivity_control.enable = true;
  sense_params.result_abbreviation.enable = true;
  sense_params.label_hiding.enable = true;

  if (sense::Init("Your project key",
                  sense_params) < 0) {
    return -1;
  }

  if (!StreamPrediction())
    std::cerr << "Stream prediction failed." << std::endl;
  sense::Terminate();
  return 0;
}

