/*
 *  This small demo presents a 30 second 1000 Hz stereo sine generation sampled at 48 kHz
 *  The sample delivery rate is configurable by the PROC_SIN_FRAME_SIZE const.
 */

#include <alsa/asoundlib.h>
#include <math.h>
#include <limits.h>
#include <array>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <arpa/inet.h>

const char *_device = "default";           /* playback device */
const size_t BUFFER_SIZE = 48000*2;  /* buffer that fits 1 second of stereo samples - sampling freq = 48 kHz */
const size_t PROC_SIN_FRAME_SIZE = 1152; /* a single processing frame contains 1152 stereo-samples which is 24 ms when sampling at 48 kHz */
const size_t PROC_SIN_FRAME_DURATION_MS = 24; /* a single processing contains 24 ms of audio */

const double DAMPENING_FACTOR = 0.70794578438413791080221494218931; /* damping by 3dB expressed in doubles */
const int16_t DUMPING_FACTOR_IN_SHORTS = 23198; /* this a part of the sample level damping by 3dB    -3dB = 20*log(23198/32767) */

uint16_t _buffer[BUFFER_SIZE] = {0};

const double _samplingRate = 48000.0;
const double _sineFrequency = 1000.0;
const double _PI = 3.14159265;
const unsigned int _simulationDurSec = 30;

template<typename T>
std::string strToInt(T i)
{
   std::stringstream stream;
   stream << i;

   return stream.str();
}

template <typename TVal, size_t size>
std::string getCommaSepNumString(TVal (&bytes)[size])
{
   std::string res;
   int i {};

   std::for_each(std::begin(bytes), std::end(bytes), [&res, &i](TVal c) {
      ++i;
      res.append(strToInt(c));
      if(i < size) { res.append(", "); }
   });

   return res;
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

    int16_t monoSine1kHzLoopUp[48] {}; // contains 1kHz sine - sampled at 48 kHz

    // putting a 1 kHz sine wave (sampling rate 48 kHz) in to the buffer
    const double angleIncrement = (_sineFrequency/_samplingRate) * 2.0 * _PI;
    double probedFreq = 0.0;
    double amplitudeVal = 0.0;
    for(size_t a = 0,iii = 0; iii < _samplingRate; a=a+2, ++iii)
    {
      probedFreq = iii * angleIncrement;
      amplitudeVal = sin(probedFreq);
      auto tempVal = static_cast<int16_t>(INT16_MAX * (DAMPENING_FACTOR*amplitudeVal));

      if(iii < 48)
      {
         monoSine1kHzLoopUp[iii] = tempVal;
      }

      _buffer[a] = *reinterpret_cast<uint16_t*>(&tempVal);
      _buffer[a+1] = *reinterpret_cast<uint16_t*>(&tempVal);
    }

    std::cout << "int16_t monoSine1kHzLoopUp[48] { " << getCommaSepNumString(monoSine1kHzLoopUp) << "};" << std::endl;

    if ((err = snd_pcm_open(&handle, _device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = snd_pcm_set_params(handle,
                      SND_PCM_FORMAT_S16_BE,
                      SND_PCM_ACCESS_RW_INTERLEAVED,
                      2,
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

   auto writeFrameSize = PROC_SIN_FRAME_SIZE; // <-- set how big should the sample chunks be that we send to ALSA
   std::vector<uint16_t> _tempWriteBuff {};
   _tempWriteBuff.reserve(writeFrameSize*2);
   auto buffSampleCount = sizeof(_buffer)/sizeof(uint16_t);
   uint16_t* currBuffPtr = nullptr;
   int writePosReminder {0};

   for (i = 0; i < _simulationDurSec; i++)
   {
      int writePos = writePosReminder;

      for (int j = 0; writePos < buffSampleCount; j++)
      {
         int nextFramePosEnd = writePos + (writeFrameSize*2);

         if(nextFramePosEnd > buffSampleCount)
         {
            _tempWriteBuff.clear();

            auto lastBuffSamplesAmount = buffSampleCount - writePos;

            std::copy_n(_buffer + writePos, lastBuffSamplesAmount, std::back_inserter(_tempWriteBuff));

            auto firstBuffSamplesAmount = (writeFrameSize*2) - lastBuffSamplesAmount;

            std::copy_n(_buffer, firstBuffSamplesAmount, std::back_inserter(_tempWriteBuff));

            writePosReminder = firstBuffSamplesAmount;

            currBuffPtr = _tempWriteBuff.data();
         }
         else
         {
            currBuffPtr = _buffer + writePos;
         }

         frames = snd_pcm_writei(handle, currBuffPtr, writeFrameSize);

         if (frames < 0)
            frames = snd_pcm_recover(handle, frames, 0);
         if (frames < 0) {
            printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
            break;
         }
         if (frames > 0 && frames < writeFrameSize)
         {
            printf("Short write (expected %li, wrote %li)\n", (long)writeFrameSize, frames);
         }
         else
         {
            //printf("ALL GOOD wrote %li frames.\n", frames);
         }

         writePos += frames*2; //incrementing write position (one frame contains two samples thus x 2 multiplication)

         // adding a thread sleep to make it a more realistic live-streaming scenario
         std::this_thread::sleep_for (std::chrono::milliseconds(PROC_SIN_FRAME_DURATION_MS));
      }
      printf("Passed audio-write iterations: %d.\n", i+1);
   }

    err = snd_pcm_drain(handle);
    if (err < 0)
        printf("snd_pcm_drain failed: %s\n", snd_strerror(err));
    snd_pcm_close(handle);

    return 0;
}
