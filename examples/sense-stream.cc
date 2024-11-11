// Copyright 2021-2024 Cochl.
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

#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/simple.h>
#include <signal.h>

#include <iostream>

#include "sense/audio_source_stream.hpp"
#include "sense/sense.hpp"

#define SAMPLE_RATE (22050)

static bool running = true;

// Signals handling
static void handle_sigterm(int signo) {
  std::ignore = signo;  // Intentionally ignore unused parameter `signo`.
  running = false;
}

void init_signal() {
  struct sigaction sa;
  sa.sa_flags = 0;

  sigemptyset(&sa.sa_mask);
  sa.sa_handler = handle_sigterm;
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGINT, &sa, nullptr);
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
  pa_simple* s = nullptr;
  int error = 0;
  if (!(s = pa_simple_new(nullptr,
                          "sense-stream",
                          PA_STREAM_RECORD,
                          nullptr,
                          "record",
                          &ss,
                          nullptr,
                          nullptr,
                          &error))) {
    fprintf(stderr,
            __FILE__ ": pa_simple_new() failed: %s\n",
            pa_strerror(error));
    return false;
  }

  // Create a sense audio stream instance
  sense::AudioSourceStream audio_source_stream;
  // The buffer size must be obtained in the following way after calling the
  // init method:
  const auto buf_size = static_cast<int>(SAMPLE_RATE * ss.channels *
                                         audio_source_stream.get_hop_size());

  std::vector<int16_t> audio_sample;
  std::vector<int16_t> buf(buf_size);

  // The Sense SDK is meant to be used with the audio frames overlapping:
  //
  //   [+ + + +]             : first frame,  0.0-2.0 s
  //       [+ + + +]         : second frame, 1.0-3.0 s
  //       ^   [+ + + +]     : third frame,  2.0-4.0 s
  //       |       [+ + + +] : fourth frame, 3.0-5.0 s
  //       |                ...
  //       1 second later
  //
  // Every iteration we will pop 1 second of audio from the front,
  // and push back 1 second.
  // The main reason to do this is to ensure we catch an event if something
  // occurs between two frames.
  const bool result_abbreviation =
      sense::get_parameters().result_abbreviation.enable;
  while (running) {
    // Record some data ...
    if (pa_simple_read(s, buf.data(), buf_size * sizeof(int16_t), &error) < 0) {
      fprintf(stderr,
              __FILE__ ": pa_simple_read() failed: %s\n",
              pa_strerror(error));
      if (s) pa_simple_free(s);
      return false;
    }

    if (audio_sample.empty()) {
      audio_sample = {buf.begin(), buf.end()};
      audio_sample.insert(audio_sample.begin() + buf_size,
                          buf.begin(),
                          buf.end());
      continue;
    }
    audio_sample.erase(audio_sample.begin(), audio_sample.begin() + buf_size);
    audio_sample.insert(audio_sample.begin() + buf_size,
                        buf.begin(),
                        buf.end());

    // Run the prediction, and it will return a 'FrameResult' object.
    sense::FrameResult frame_result =
        audio_source_stream.Predict(audio_sample, SAMPLE_RATE);
    if (!frame_result) {
      std::cerr << frame_result.error << std::endl;
      break;
    }

    if (result_abbreviation) {
      for (const auto& abbreviation : frame_result.abbreviations)
        std::cout << abbreviation << std::endl;
      // Even if you use the result abbreviation, you can still get precise
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

  sense_params.sensitivity_control.enable = true;
  sense_params.result_abbreviation.enable = true;

  if (sense::Init("Your project key", sense_params) < 0) return -1;

  if (!StreamPrediction())
    std::cerr << "Stream prediction failed." << std::endl;
  sense::Terminate();
  return 0;
}

