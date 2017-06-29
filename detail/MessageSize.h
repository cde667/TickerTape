#pragma once

#include <cstddef>
#include "Utils.h"

namespace detail
{
    // ----------- Message size -------------
    template< typename... TMsgTypes >
    struct MessageSize;

    template< typename TMsgType, typename... TMsgTypes >
    struct MessageSize< TMsgType, TMsgTypes... > 
        : public MessageSize< TMsgTypes...>
    {
        static constexpr size_t thisMsgSize = sizeof(TMsgType);
        static constexpr size_t msgSize = ceMAX( thisMsgSize, MessageSize< TMsgTypes... >::msgSize );
    };

    template<>
    struct MessageSize<>
    {
        static const size_t msgSize = 0;
    };
}


