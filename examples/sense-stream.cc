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

void finish(pa_simple *s) {
  if (s)
    pa_simple_free(s);
}

static bool running = true;

/* Signals handling */
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

// To run this example, install pulseaudio on your machine
// sudo apt-get install -y libpulse-dev
// Make sure pulseaudio is set on a valid input
// -> $ pacmd list-sources | grep -e 'index:' -e device.string -e 'name:'
// To change the default source -> $ pacmd set-default-source "SOURCE_NAME"
// It's also very common that pulse audio is not starting correctly.
// pulseaudio -k #kill the process just in case
// pulseaudio -D #start it again
int main(int argc, char *argv[]) {
  static pa_sample_spec ss;
  ss.format = PA_SAMPLE_S16LE;  // May vary based on your system
  ss.rate = SAMPLE_RATE;
  ss.channels = 1;

  init_signal();

  // Init the sense
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

  // Create a sense audio stream instance
  sense::AudioSourceStream audio_source_stream;
  std::vector<float> audio_sample(SAMPLE_RATE);
  bool first_frame = true;

  pa_simple *s = NULL;
  int error;
  // Create the recording stream
  if (!(s = pa_simple_new(NULL, argv[0],
                          PA_STREAM_RECORD, NULL,
                          "record", &ss,
                          NULL, NULL, &error))) {
    fprintf(stderr,
            __FILE__": pa_simple_new() failed: %s\n",
            pa_strerror(error));
    finish(s);
    return -1;
  }


  /*
  The sense is meant to be used with the audio frames overlapping
  [ + + + + ] first frame
       [+ + + +] second frame
       ^    [+ + + +] third frame
       |        [+ + + +] fourth frame
       0.5 second later
  This is why our buffer length is set with SAMPLE_RATE / 2.
  Every iteration we will pop 0.5s of audio from the front, and push back
  0.5 sec.
  The main reason to do this is to ensure we catch an event if something
  occurs between two frames.
  */
  for (; running;) {
    int16_t buf[BUF_SIZE];

    /* Record some data ... */
    if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
        fprintf(stderr,
                __FILE__": pa_simple_read() failed: %s\n",
                pa_strerror(error));
        finish(s);
        return -1;
    }
    std::cout << "---------NEW FRAME---------" << std::endl;
    if (first_frame) {
      audio_sample = {buf, buf + BUF_SIZE};
      audio_sample.insert(audio_sample.begin() + BUF_SIZE,
                          std::begin(buf), std::end(buf));
      first_frame = false;
    } else {
      audio_sample.erase(audio_sample.begin(),
                          audio_sample.begin() + BUF_SIZE);
      audio_sample.insert(audio_sample.begin() + BUF_SIZE,
                          std::begin(buf), std::end(buf));
      // Run the prediction
      auto frame_result = audio_source_stream.Predict(audio_sample);
      std::cout << frame_result << std::endl;
    }
  }
  finish(s);
  sense::Terminate();
  return 0;
}
