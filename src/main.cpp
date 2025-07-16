#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>

#include "AvMedium.h"

void save_frame_seq(AVFrame* frame)
{
    static SwsContext* sws_ctx = nullptr;
    static cv::Mat     img_bgr;
    if (!sws_ctx)
    {
        sws_ctx = sws_getContext(
            frame->width, frame->height, (AVPixelFormat)frame->format,
            frame->width, frame->height, AV_PIX_FMT_BGR24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        img_bgr = cv::Mat(frame->height, frame->width, CV_8UC3);
    }
    uint8_t* dst_data[1]     = {img_bgr.data};
    int      dst_linesize[1] = {static_cast<int>(img_bgr.step[0])};
    sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, dst_data, dst_linesize);
    cv::imshow("Video", img_bgr);
    if (cv::waitKey(1) == 27)
    {
        // ESC 退出
        exit(0);
    }
}

int main(int /*argc*/, char const* /*argv*/[])
{
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    AvMedium av{};
    av.setStreamUrl(R"(rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid)");
    av.mediumStart();
    auto gen = av.flushPacket();
#if false
    while (gen.next())
    {
        AVFrame* frame = gen.current();
        save_frame_seq(frame);
    }
#elif true
    std::thread{
        [&gen]() {
            while (gen.next())
            {
                AVFrame* frame = gen.current();
                save_frame_seq(frame);
            }
        }}
        .detach();
    while (true)
    {
        std::cout << "....\n";
    }
#endif
    return 0;
}

// rtmp://ns8.indexforce.com/home/mystream  伊拉克直播电视台
// rtsp://77.110.228.219/axis-media/media.amp
// rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid
// http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/gear2/prog_index.m3u8
