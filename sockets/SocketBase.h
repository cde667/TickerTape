

namespace sockets
{

    template< size_t BUFFER_SIZE = 2048 >
    class SocketBase
    {
        int mFd;            // socket file descriptor
        sockaddr_in mAddr;  // Socket address

        int mSockFamily;
        int mSockType;
        int mSockProto;
        struct timeval mLastRecv;

        char mBuffer[ BUFFER_SIZE ];
        ssize_t mBytes;

      public:
        SocketBase( int family, int type, int proto )
            : mFd(-1), mSockFamily(family), mSockType(type)
            , mSockProto(proto), mBytes(0)
        {
            gettimeofday( &mLastRecv, nullptr );
            memset( mBuffer, 0, sizeof(mBuffer) );
            memset( mAddr, 0, sizeof(mAddr) );
        }

        ~SocketBase(void) 
        {
            if (mFd != -1) {
                close(mFd);
                mFd = -1;
            }
        }

        // set - set nonblocking, ...
        // read
        // connect
        // accept
        // listen
        // bind

    };

}
