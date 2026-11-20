#pragma once

extern "C"
{
    #include <libavutil/frame.h>
    #include <libswresample/swresample.h>
}

#include "api/MediaTypes.h"

#include <cstddef>
#include <string>
#include <vector>

namespace player
{
    class AudioResampler
    {
    public:
        AudioResampler() = default;
        ~AudioResampler();

        bool open(const AVFrame &inputFrame, const AudioFormat &targetFormat, std::string &error);
        bool resample(const AVFrame &inputFrame, std::vector<std::byte> &output, std::string &error);
        bool drain(std::vector<std::byte> &output, std::string &error);
        void reset();
        bool isPrepared() const { return context_ != nullptr; }

    private:
        static std::string fferr(int code);

        SwrContext *context_{nullptr};
        AudioFormat targetFormat_{};
    };
}
