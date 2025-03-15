#include <iostream>

extern "C" {
#include <libavutil/log.h>
#include <libavutil/mem.h>
}

int main(int argc, char const* argv[])
{
    av_log_set_level(AV_LOG_DEBUG);
    int* ptr = static_cast<int*>(av_malloc(sizeof(int)));
    *ptr     = 101;
    std::cout << *ptr << '\n';
    av_free(static_cast<void*>(ptr));
    std::cout << *ptr << '\n';
    ptr = nullptr;
    if (!ptr)
    {
        std::cout << "free" << '\n';
    }
    else
    {
        std::cout << "no free" << '\n';
    }
    return 0;
}
