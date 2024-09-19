/*
 *   This small demo presents a 10 second 1000 Hz mono sine generation sampled at 48 kHz
 *   Furthermore it shows how one can tackle the problem of bit resolution change from 24 bit to 16 bit
 */

#include <alsa/asoundlib.h>
#include <math.h>
#include <limits.h>
#include <arpa/inet.h>
#include <iterator>
#include <algorithm>

const char *device = "default";            /* playback device */

uint16_t _buffer[48*1000];

const double _samplingRate = 48000.0;
const double _sineFrequency = 1000.0;
const double _PI = 3.14159265;
const unsigned int _simulationDurSec = 20;
const int32_t INT24_MAX = 8388607;

int16_t getTruncatedSample(const int32_t audioSample24bit)
{
   auto res = audioSample24bit;

   res = ((res + 128) >> 8);

   if(res < INT16_MIN)
   {
      res = INT16_MIN;
   }
   else if(res > INT16_MAX)
   {
      res = INT16_MAX;
   }

   // damping by 1dB to avoid clipping
   res = (res * 29204) / 32768;

   return (int16_t) res;
}

bool runningOnLittleEndianHost()
{
   return htonl(23) != 23;
}

template <typename TVal, size_t size>
void convToBigEndian(TVal (&buff)[size])
{
   std::for_each(std::begin(buff), std::end(buff), [](uint16_t& i) { i = htons(i); });
}

int main(void)
{
    int err;
    unsigned int i;
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;

    // putting a 1 kHz sine wave (sampling rate 48 kHz) in to the buffer
    const double angleIncrement = (_sineFrequency/_samplingRate) * 2.0 * _PI;
    double probedFreq = 0.0;
    double amplitudeVal = 0.0;
    for(int a = 0; a < _samplingRate; ++a)
    {
        probedFreq = a * angleIncrement;
        amplitudeVal = sin(probedFreq);
        _buffer[a] = (short) getTruncatedSample(INT24_MAX * amplitudeVal);
    }

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = snd_pcm_set_params(handle,
                      SND_PCM_FORMAT_S16_BE,
                      SND_PCM_ACCESS_RW_INTERLEAVED,
                      1,
                      48000,
                      1,
                      5000000)) < 0) {   /* 0.5sec */
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

   if(runningOnLittleEndianHost())
   {
      convToBigEndian(_buffer);
   }

   for (i = 0; i < _simulationDurSec; i++)
   {
      frames = snd_pcm_writei(handle, _buffer, sizeof(_buffer)/sizeof(short));

      if (frames < 0)
         frames = snd_pcm_recover(handle, frames, 0);
      if (frames < 0) {
         printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
         break;
      }
      if (frames > 0 && frames < (long)sizeof(_buffer)/sizeof(short))
      {
         printf("Short write (expected %li, wrote %li)\n", (long)sizeof(_buffer), frames);
      }
      else
      {
         printf("ALL GOOD wrote %li frames.\n", frames);
      }
   }

    /* pass the remaining samples, otherwise they're dropped in close */
    err = snd_pcm_drain(handle);
    if (err < 0)
        printf("snd_pcm_drain failed: %s\n", snd_strerror(err));
    snd_pcm_close(handle);
    return 0;
}
