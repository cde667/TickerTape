#pragma once

#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>

namespace sockets
{

    //
    // Listen for inbound connections
    //
    class SyncSocketListener
    {
        int mFd;
        int mSockFamily;
        int mSockType;
        int mSockProto;
        sockaddr mAddr;

      public:
        SyncSocketListener( int family, int type, int proto );

        void set( const std::string& addr, int port );

        // Connect opens up a socket to listen on...
        bool connect(void);

#if 0
        // poll
        void poll( );
#endif

        // Send a data buffer : we only poll, so NONE!
        void sendBuffer( uint64_t, const void*, size_t )
        {}

        // read : create a new inbound connection
        template< typename TSyncServer >
        bool read( TSyncServer& sync );

    };

} // namespace socket
