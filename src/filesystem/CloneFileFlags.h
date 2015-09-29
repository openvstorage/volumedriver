#ifndef _VOLUMEDRIVERFS_CLONEFILE_FLAGS_H
#define _VOLUMEDRIVERFS_CLONEFILE_FLAGS_H

namespace volumedriverfs
{

enum class CloneFileFlags
{
    Guarded = (1 << 0),
    Lazy = (1 << 1),
    Valid = (1 << 2),
    SkipZeroes = (1 << 4)
};

inline bool
operator& (CloneFileFlags a, CloneFileFlags b)
{
    return static_cast<bool>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
}

inline CloneFileFlags
operator| (CloneFileFlags a, CloneFileFlags b)
{
    return static_cast<CloneFileFlags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

} //namespace volumedriverfs


#endif //_VOLUMEDRIVERFS_CLONEFILE_FLAGS_H
