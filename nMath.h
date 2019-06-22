// nMath - simple math utils
// Author - Nic Taylor

#pragma once

namespace nMath {
    template <typename T>
    inline T Max(T lhs, T rhs)
    {
        return (lhs > rhs) ? lhs : rhs;
    }

    template <typename T>
    inline T Min(T lhs, T rhs)
    {
        return (lhs < rhs) ? lhs : rhs;
    }

    template <typename T>
    inline T Clamp(T _val, T _min, T _max)
    {
        return Max(_min, Min(_max, _val));
    }
}
