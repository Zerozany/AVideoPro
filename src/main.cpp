#include "AvMedium.h"
#include "Version.hpp"

int main(int argc, char const* argv[])
{
    AvMedium av{R"(rtmp://ns8.indexforce.com/home/mystream )"};
    // std::cout << AVideoPro::Version() << '\n';
    av.read();
    return 0;
}

// rtmp://ns8.indexforce.com/home/mystream  伊拉克直播电视台
// rtsp://77.110.228.219/axis-media/media.amp
// rtsp://172.16.6.100:554/stream0
// rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid
// http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/gear2/prog_index.m3u8
