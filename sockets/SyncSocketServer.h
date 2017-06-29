#pragma once

#include <cstdint>
#include <sys/socket.h>

namespace sockets
{

    //
    // Server Socket : here, we SEND DATA out
    //
    class SyncSocketServer
    {
        int mFd;
        int64_t mMaxAck;

        int mSockFamily;
        int mSockType;
        int mSockProto;
        sockaddr mAddr;
        struct timeval mLastRecv;

        static const size_t BUFFER_SIZE = 2048;
        char mBuffer[BUFFER_SIZE];
        size_t mBytes = 0;

        template< typename TapeSync >
        size_t parse( TapeSync& sync, void* buffer, size_t bytes );

      public:
        // Created when we get an inbound connection...
        SyncSocketServer( int family, int type, int proto, int fd, sockaddr addr );

        void sendBuffer( uint64_t seq, const void* buffer, size_t bytes );

        void sendHeartbeat();
        
        void sendKeepAlive();

        template< typename TapeSync >
        size_t read( TapeSync& sync );
    };

} // namespace socket
