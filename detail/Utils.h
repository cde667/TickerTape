#pragma once


namespace detail
{
    constexpr size_t ceMAX( size_t s1, size_t s2 )
    {
        return (s1>s2 ? s1 : s2 );
    }

    constexpr size_t ceMIN( size_t s1, size_t s2 )
    {       
        return (s1<s2 ? s1 : s2 );
    }

}
