#include <QApplication>
#include <QDir>
#include <QImage>
#include <print>
#include <thread>

#include "MediaFrame.h"
#include "MediaPlay.h"

auto FrameImageTest() -> void
{
    MediaFrame* mediaFrame = new MediaFrame{R"(rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid)"};
    if (!mediaFrame->start())
    {
        return;
    }
    std::thread{[&mediaFrame] {
        QDir().mkpath("./images");
        int  index = 0;
        auto gen   = mediaFrame->flushPacket();
        while (gen.nextValue())
        {
            auto   data = gen.current();
            QImage img{data.rgbBuffer.data(), data.width, data.height, data.lineSize, QImage::Format_RGB888};

            QString filename = QString("./images/frame_%1.png").arg(index++, 5, 10, QLatin1Char('0'));
            img.save(filename);
        }
    }}.detach();

    std::thread{[&mediaFrame] {
        std::this_thread::sleep_for(std::chrono::seconds{5});
    }}.join();

    delete mediaFrame;
}

int main(int argc, char* argv[])
{
    QApplication app{argc, argv};
    MediaPlay    mediaPlay{};
    mediaPlay.resize(1280, 720);
    mediaPlay.show();

    mediaPlay.setUrl(R"(rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid)");
    std::thread{[&mediaPlay] {
        mediaPlay.play();
    }}.detach();

    std::thread{[&mediaPlay] {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        mediaPlay.setUrl(R"(rtmp://ns8.indexforce.com/home/mystream)");
        mediaPlay.play();
    }}.detach();

    QApplication::exec();
}
