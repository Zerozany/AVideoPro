#include "FormatAV.h"

int main(int argc, char const* argv[])
{
    FormatAV av{R"(F:\DeskTop\Destiny2\test01.mp4)"};
    av.read();
    return 0;
}

// rtmp://ns8.indexforce.com/home/mystream  伊拉克直播电视台
// rtsp://77.110.228.219/axis-media/media.amp
