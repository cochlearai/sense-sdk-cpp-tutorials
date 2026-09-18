// Single translation unit that compiles the miniaudio implementation.
// miniaudio is a vendored, single-header, public-domain/MIT-0 audio capture
// library (https://miniaud.io). Only the sense-stream example uses it, to read
// from the host microphone; the sense SDK itself has no audio-device
// dependency. Keeping the implementation in its own C file lets every other
// translation unit include miniaudio.h for declarations only.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
