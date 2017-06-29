#pragma once

#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

namespace sockets
{
    class SyncSocketClient
    {
      protected:
        int mFd;            // socket file descriptor
        sockaddr_in mAddr;  // Socket address

        uint64_t mNextAck;  // ACK to send *next*
        uint64_t mLastAck;  // Last ACK we sent
       
        int mSockFamily;
        int mSockType;
        int mSockProto;
        struct timeval mLastRecv;

        static const size_t BUFFER_SIZE = 2048;
        char mBuffer[ BUFFER_SIZE ];
        ssize_t mBytes = 0;

      private:
        // Parse up an incoming buffer
        template< typename TapeSync > 
        size_t parse( TapeSync& sync, void* buffer, size_t bytes );

      public:
        SyncSocketClient( int family, int type, int proto );

        void set( const std::string& addr, int port );

        bool connect(void);

        void sendBuffer( uint64_t, const void*, size_t )
        {}

        // Send an ACK back to the server for all the packets we just processed
        void sendAck();

        // Send a routine heartbeat...
        void sendHeartbeat();

        // Send a keep alive packet...
        void sendKeepAlive();

        // Read : grab data from the network and write it to our local tape.
        template< typename TapeSync >
        size_t read( TapeSync& sync );

    };

}
