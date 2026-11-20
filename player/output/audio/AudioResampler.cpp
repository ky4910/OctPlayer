#include "output/audio/AudioResampler.h"

extern "C"
{
    #include <libavutil/channel_layout.h>
    #include <libavutil/error.h>
    #include <libavutil/samplefmt.h>
}

#include <cstdint>
#include <cstring>

namespace player
{
    std::string AudioResampler::fferr(int code)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(code, buffer, sizeof buffer);
        return buffer;
    }

    AudioResampler::~AudioResampler()
    {
        reset();
    }

    bool AudioResampler::open(const AVFrame &inputFrame, const AudioFormat &targetFormat, std::string &error)
    {
        reset();

        AVChannelLayout inLayout = inputFrame.ch_layout;
        AVChannelLayout outLayout{};
        av_channel_layout_default(&outLayout, targetFormat.channels);

        context_ = swr_alloc();
        if (!context_)
        {
            error = "swr_alloc failed";
            return false;
        }

        const int ret = swr_alloc_set_opts2(
            &context_,
            &outLayout,
            AV_SAMPLE_FMT_S16,
            targetFormat.sampleRate,
            &inLayout,
            static_cast<AVSampleFormat>(inputFrame.format),
            inputFrame.sample_rate,
            0,
            nullptr);
        if (ret < 0)
        {
            error = fferr(ret);
            reset();
            return false;
        }

        if (swr_init(context_) < 0)
        {
            error = "swr_init failed";
            reset();
            return false;
        }

        targetFormat_ = targetFormat;
        return true;
    }

    bool AudioResampler::resample(const AVFrame &inputFrame, std::vector<std::byte> &output, std::string &error)
    {
        if (!context_)
        {
            error = "resampler is not opened";
            return false;
        }

        const int outSamples = av_rescale_rnd(
            swr_get_delay(context_, inputFrame.sample_rate) + inputFrame.nb_samples,
            targetFormat_.sampleRate,
            inputFrame.sample_rate,
            AV_ROUND_UP);

        uint8_t *buffer = nullptr;
        int lineSize = 0;
        const int sampleBytes = sizeof(std::int16_t);
        const int bytes = outSamples * targetFormat_.channels * sampleBytes;

        output.resize(static_cast<std::size_t>(bytes));
        if (bytes == 0)
            return true;

        const int ret = av_samples_alloc(&buffer, &lineSize, targetFormat_.channels, outSamples, AV_SAMPLE_FMT_S16, 0);
        if (ret < 0)
        {
            error = fferr(ret);
            return false;
        }

        const int converted = swr_convert(
            context_,
            &buffer,
            outSamples,
            const_cast<const uint8_t **>(inputFrame.data),
            inputFrame.nb_samples);

        if (converted < 0)
        {
            av_freep(&buffer);
            error = fferr(converted);
            return false;
        }

        const int actualBytes = converted * targetFormat_.channels * sampleBytes;
        std::memcpy(output.data(), buffer, static_cast<std::size_t>(actualBytes));
        output.resize(static_cast<std::size_t>(actualBytes));

        av_freep(&buffer);
        return true;
    }

    bool AudioResampler::drain(std::vector<std::byte> &output, std::string &error)
    {
        output.clear();
        if (!context_)
            return true;

        const int delay = static_cast<int>(swr_get_delay(context_, targetFormat_.sampleRate));
        if (delay <= 0)
            return true;

        uint8_t *buffer = nullptr;
        int lineSize = 0;
        const int ret = av_samples_alloc(
            &buffer, &lineSize, targetFormat_.channels, delay, AV_SAMPLE_FMT_S16, 0);
        if (ret < 0)
        {
            error = fferr(ret);
            return false;
        }

        const int converted = swr_convert(context_, &buffer, delay, nullptr, 0);
        if (converted < 0)
        {
            av_freep(&buffer);
            error = fferr(converted);
            return false;
        }

        const auto bytes = static_cast<std::size_t>(converted) *
                           targetFormat_.channels * sizeof(std::int16_t);
        output.resize(bytes);
        if (bytes > 0)
            std::memcpy(output.data(), buffer, bytes);
        av_freep(&buffer);
        return true;
    }

    void AudioResampler::reset()
    {
        if (context_)
        {
            swr_free(&context_);
            context_ = nullptr;
        }
        targetFormat_ = {};
    }
}
