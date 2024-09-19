/*
 *  This small demo presents a 10 second 1000 Hz stereo sine generation sampled at 48 kHz
 *  Presents a more resource efficient way of generating a sine
 *  Example can be usefull in embedded enviorments with limited resources, which are often
 *  compiled using an older C++ standards dissalowing extensive compile-time programming using constexpr
 */

#include <alsa/asoundlib.h>
#include <math.h>
#include <limits.h>
#include <array>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <iostream>
#include <arpa/inet.h>

namespace params
{
   const char*    AUD_DEVICE = "default";      /* playback device */
   const uint32_t SAMPLE_RATE = 48000;         /* sampling rate in Hz */
   const uint32_t AUD_CHANNELS = 2;            /* specifies the number to of output audio channels */
   const uint32_t SINE_FREQ = 1000;            /* amount of sine waves per second expressed in Hz */
   const uint32_t SAMPLES_PER_SINE = SAMPLE_RATE/SINE_FREQ; /* amount of samples of per sine wave of the given frequency */
   const uint32_t PLAYBACK_TIME_SEC = 40;      /* specified the duration of the playback in seconds*/
   const uint32_t PROC_FRAME_SIZE = 1152;      /* a single processing frame contains 1152 stereo-samples which is 24 ms when sampling at 48 kHz */
   const uint32_t PROC_FRAME_DURATION_MS = 24; /* a single processing contains 24 ms of audio */
   const uint32_t SINES_IN_FRAME = PROC_FRAME_SIZE / SAMPLES_PER_SINE; /* how many sines fit to a single frame - this should be round value*/

   static_assert( SINE_FREQ < SAMPLE_RATE/2 ); /* probed signal frequency should be smaller than half of the sampling frequency (nyquist frequncy) */

   static_assert( (PROC_FRAME_SIZE % SAMPLES_PER_SINE) == 0 ); /* Current program version has the limitation in favor of simplicity. */
                                                                       /* Frame size should be a multiple of the sine samples*/
}

void listdev(char *devname)
{
    char** hints;
    int    err;
    char** n;
    char*  name;
    char*  desc;
    char*  ioid;

    /* Enumerate sound devices */
    err = snd_device_name_hint(-1, devname, (void***)&hints);
    if (err != 0) {

        fprintf(stderr, "*** Cannot get device names\n");
        exit(1);

    }
    n = hints;
    while (*n != NULL) {

        name = snd_device_name_get_hint(*n, "NAME");
        desc = snd_device_name_get_hint(*n, "DESC");
        ioid = snd_device_name_get_hint(*n, "IOID");

        printf("Name of device: %s\n", name);
        printf("Description of device: %s\n", desc);
        printf("I/O type of device: %s\n", ioid);
        printf("\n");
        if (name && strcmp("null", name)) free(name);
        if (desc && strcmp("null", desc)) free(desc);
        if (ioid && strcmp("null", ioid)) free(ioid);
        n++;

    }

    //Free hint buffer too
    snd_device_name_free_hint((void**)hints);
}

bool runningOnLittleEndianHost()
{
   return htonl(23) != 23;
}

template <size_t size>
std::vector<uint16_t> fillBuffer(int16_t (&monoSigBuff)[size], std::vector<uint16_t>& buf)
{
   for (uint32_t i = 0u; i < params::SINES_IN_FRAME; i++)
   {
      std::for_each(std::begin(monoSigBuff), std::end(monoSigBuff), [&buf](int16_t& s)
      {
         for(auto o = 0u; o < params::AUD_CHANNELS; ++o)
         {
            buf.push_back(s);
         }
      });
   }

   return buf;
}

std::vector<uint16_t> convToBigEndian(std::vector<uint16_t>& buff)
{
   std::for_each(buff.begin(), buff.end(), [](uint16_t& i) { i = htons(i); });

   return buff;
}

template <size_t size>
std::vector<uint16_t> prepWriteBuffer(int16_t (&sigBuff)[size])
{
   std::vector<uint16_t> buff {};

   buff = fillBuffer(sigBuff, buff);

   if(runningOnLittleEndianHost)
   {
      buff = convToBigEndian(buff);
   }

   return buff;
}

int main(void)
{
    int err;
    unsigned int i;
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;

    // For enviorments with very little memory - this sample sine array could contain only 1/4 - 12 samples.
   int16_t monoSine1kHzLoopUp[48]  { 0, 3027, 6003, 8877, 11598, 14121, 16402, 18403, 20089, 21431, 22406, 22998, 23197, 22998,
                                     22406, 21431, 20089, 18403, 16402, 14121, 11598, 8877, 6003, 3027, 0, -3027, -6003, -8877,
                                     -11598, -14121, -16402, -18403, -20089, -21431, -22406, -22998, -23197, -22998, -22406,
                                     -21431, -20089, -18403, -16402, -14121, -11598, -8877, -6003, -3027};

   std::vector<uint16_t> tempWriteBuff {};

   //listdev("pcm");

   const auto endianStr = runningOnLittleEndianHost() ? "little endian" : "big endian";

   std::cout << "CPU is using: " << endianStr << std::endl;

    if ((err = snd_pcm_open(&handle, params::AUD_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = snd_pcm_set_params(handle,
                      SND_PCM_FORMAT_S16_BE,
                      SND_PCM_ACCESS_RW_INTERLEAVED,
                      params::AUD_CHANNELS,
                      params::SAMPLE_RATE,
                      1,
                      5000000)) < 0) {   /* 0.5sec */
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

   tempWriteBuff = prepWriteBuffer(monoSine1kHzLoopUp);

   std::cout << "tempWriteBuff = " << tempWriteBuff.size() << std::endl;


   for (uint32_t i = 0u; i < params::PLAYBACK_TIME_SEC; ++i)
   {
      // In the case if sample rate is not a multitude of the frame - the following loop will deliver slightly less
      // audio-data than one second - imperfaction left on purpose so to keep it simple and reduce the human cognitive load of the reader
      for (uint32_t j = 0u; j < (params::SAMPLE_RATE / params::PROC_FRAME_SIZE); ++j)
      {
         frames = snd_pcm_writei(handle, tempWriteBuff.data(), params::PROC_FRAME_SIZE);

         if (frames < 0)
            frames = snd_pcm_recover(handle, frames, 0);
         if (frames < 0) {
            printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
            break;
         }
         if ((frames > 0) && (frames < params::PROC_FRAME_SIZE))
         {
            printf("Short write (expected %li, wrote %li)\n", (long)params::PROC_FRAME_SIZE, frames);
         }

         // adding a thread sleep to make it a more realistic live-streaming scenario
         std::this_thread::sleep_for (std::chrono::milliseconds(params::PROC_FRAME_DURATION_MS));
      }
      printf("Passed audio-write iterations: %d.\n", i+1);
   }

    err = snd_pcm_drain(handle);
    if (err < 0)
        printf("snd_pcm_drain failed: %s\n", snd_strerror(err));
    snd_pcm_close(handle);

    return 0;
}
