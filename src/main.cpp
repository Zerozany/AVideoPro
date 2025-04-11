#include "AvMedium.h"

int main(int argc, char const* argv[])
{
    AvMedium av{R"(rtsp://172.16.6.100:554/stream0)"};
    av.read();
    return 0;
}

// rtmp://ns8.indexforce.com/home/mystream  伊拉克直播电视台
// rtsp://77.110.228.219/axis-media/media.amp
// rtsp://172.16.6.100:554/stream0
// rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid
