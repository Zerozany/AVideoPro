_Pragma("once");

namespace AVideoPro
{
    struct Version
    {
    public:
        constexpr operator const char*() const noexcept
        {
            return version;
        }

    private:
        inline static constexpr const char* version{"1.0.0"};
    };

}  // namespace AVideoPro