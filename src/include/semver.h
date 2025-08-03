class SemVer
{
public:
    SemVer()
    {
        major = 0;
        minor = 0;
        patch = 0;
    };

    SemVer(int version[])
    {
        major = version[0];
        minor = version[1];
        patch = version[2];
    }
    
    SemVer(int major, int minor, int patch)
    {
        this->major = major;
        this->minor = minor;
        this->patch = patch;
    }

    inline int GetMajor() { return major; };
    inline int GetMinor() { return minor; };
    inline int GetPatch() { return patch; };

    inline bool operator<(const SemVer a)
    {
        return a.major < major ||
                (a.major == major && a.minor < minor) ||
                (a.major == major && a.minor == minor && a.patch < patch);
    };

    inline bool operator>(const SemVer a)
    {
        return a.major > major ||
                (a.major == major && a.minor > minor) ||
                (a.major == major && a.minor == minor && a.patch > patch);
    };

    inline bool operator=(const SemVer a)
    {
        return a.major == major &&
                a.minor == minor &&
                a.patch == patch;
    };

    inline bool operator<=(const SemVer a)
    {
        return a.major <= major ||
                (a.major == major && a.minor <= minor) ||
                (a.major == major && a.minor == minor && a.patch <= patch);
    };

    inline bool operator>=(const SemVer a)
    {
        return a.major >= major ||
                (a.major == major && a.minor >= minor) ||
                (a.major == major && a.minor == minor && a.patch >= patch);
    };
private:
    int major;
    int minor;
    int patch;
};