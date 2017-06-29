#pragma once

#include <sys/socket.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "../DBG.h"

namespace sockets
{
    SyncSocketServer::SyncSocketServer( int family, int type, int proto, int fd, sockaddr addr )
            : mSockFamily(family), mSockType(type), mSockProto(proto), mFd(fd)
    { 
        memcpy( &mAddr, &addr, sizeof(addr) ); 
        mMaxAck = (-1);
        
        int flags = fcntl(mFd, F_GETFL, 0);
        if (flags >=0) {
            flags = (flags|O_NONBLOCK);
            fcntl(mFd, F_SETFL, flags);
            std::cerr << "Could not SET non-blocking " << strerror(errno) << std::endl;
        }
        else {
            std::cerr << "Could not GET non-blocking " << strerror(errno) << std::endl;
        }
    }     


    template< typename TapeSync >
    size_t SyncSocketServer::parse( TapeSync& sync, void* vbuffer, size_t vbytes )
    {
        size_t retV = 0;
        size_t bytes = vbytes;
        char* buffer = (char*) vbuffer;
        bool processed = false;

        do {
            SocketMsgs::Header& h = *(SocketMsgs::Header*)(buffer);
            _DBG_( std::cout << "DBG : PARSE : " << h.msgType << std::endl; );
            processed = false;
        
            switch( h.msgType )
            {
                case  SocketMsgs::eHeartbeat:
                    if (bytes >= sizeof(SocketMsgs::Heartbeat) ) {
                        retV += sizeof( SocketMsgs::Heartbeat );
                        processed = true;
                    }
                    break;

                case  SocketMsgs::eKeepAlive:
                    if (bytes >= sizeof(SocketMsgs::KeepAlive) ) {
                        sendHeartbeat();
                        retV += sizeof( SocketMsgs::KeepAlive );
                        processed = true;
                    }
                    break;

                case  SocketMsgs::eTapeBuffer:
                    // An error : I should never see these
                    break;

                case  SocketMsgs::eAck:
                    if (bytes >= sizeof(SocketMsgs::Ack)) {
                        SocketMsgs::Ack& ack = *(SocketMsgs::Ack*)(buffer);
                        uint64_t begin = ack.mAckSeqBegin;
                        uint64_t end = ack.mAckSeqEnd;
                        mMaxAck = end;
                        retV += sizeof(SocketMsgs::Ack);
                        _DBG_(std::cout << "DBG : ACK to " << mMaxAck << "\n"; );
                        processed = true;
                    }
                    break;

                case  SocketMsgs::eResendRequest:
                    if (bytes >= sizeof(SocketMsgs::ResendRequest)) {
                        SocketMsgs::ResendRequest& resend = * (SocketMsgs::ResendRequest*)(buffer);
                        uint64_t start = resend.mBeginSeq;
                        uint64_t end = resend.mEndSeq;

                        sync.processResend( *this, start, end );
                        retV += sizeof(SocketMsgs::ResendRequest);
                        processed = true;
                    }
                    break;

                default:
                    throw std::runtime_error("Corrupt!");
            }

            buffer = (char*) vbuffer + retV;
            bytes = vbytes - retV;

        } while (processed && bytes > 0);

        return retV;
    }

    //
    // Send a buffer from our tape...
    //
    void SyncSocketServer::sendBuffer( uint64_t seq, const void* buffer, size_t bytes )
    {
        SocketMsgs::SendBuffer msgHdr( seq, bytes );

        struct iovec iov[2] = { 
                                  { &msgHdr, sizeof(msgHdr) }, 
                                  { const_cast<void*>(buffer), bytes}   // Ugly.  But this *is* const while iovec is generic
                              };

        struct msghdr msg 
        {
            0, 0, // &mAddr, sizeof(mAddr),     // destination
            iov, 2,         // The data, len
            0, 0,           // access rights; not for inet.
            0
        };

        ssize_t result = sendmsg( mFd, &msg, O_NONBLOCK );
        _DBG_( std::cout << "DBG : " << seq << " Sending packet : " << result 
                         << ", expected "  << (sizeof(msgHdr) + bytes) << std::endl; );

        if ( result != sizeof(msgHdr) + bytes ) {
            std::cout << "Error : result " << result << " != " << (sizeof(mAddr) + bytes) << std::endl;
            std::cout << "      : " << sizeof(msgHdr) << " + " << bytes << "\n";
            std::cout << "      : " << sizeof(mAddr) << "\n";
        }

    }

    //
    // Send a heartbeat..
    //
    void SyncSocketServer::sendHeartbeat()
    {
        SocketMsgs::Heartbeat h;
        ssize_t result = send( mFd, &h, sizeof(h), O_NONBLOCK );
        if (result != sizeof(h)) {
            std::cerr << "Failed to write heartbeat message..." << std::endl;
        }
    }

    //
    // send a keep alive...
    //
    void SyncSocketServer::sendKeepAlive()
    {
        SocketMsgs::KeepAlive ka;
        size_t result = send( mFd, &ka, sizeof(ka), O_NONBLOCK );
        if (result != sizeof(ka)) {
            std::cerr << "Failed to write heartbeat message..." << std::endl;
        }
    }

    template< typename TapeSync >
    size_t SyncSocketServer::read( TapeSync& sync ) // TODO : void return.  
    {
        ssize_t result;
        int64_t maxAck = mMaxAck;

        while( (result=recv(mFd, mBuffer + mBytes, BUFFER_SIZE - mBytes, 0)) > 0) {
            _DBG_(std::cout << "DBG : recv : got " << result << ".\n"; );
            size_t consumed = parse( sync, mBuffer, result + mBytes);

            // Shift all bytes back, adjust mBytes
            if (result + mBytes > consumed) {
                memmove( mBuffer, mBuffer+consumed, (result + mBytes) - consumed );
                mBytes = (result + mBytes) - consumed;
            }
            else {
                mBytes = 0;
            }
        }

        if (result< 0) {
            if (errno == EAGAIN) {
                ;
            }
            else {
                std::cerr << "Socket Error : " << strerror(errno) << "\n";
                // TODO : Error handling
                throw std::runtime_error( strerror(errno) );
            }
        }
        else if (result== 0 && mSockType == SOCK_STREAM) {
            // Connection closed
            sync.removeSync( *this );
        }

        //
        // Process all ACKs at once...
        //
        if (mMaxAck != maxAck) {
            _DBG_(std::cout << "Done with Read loop.  Processing Acks : " << maxAck << " to " << mMaxAck << std::endl;);
            sync.processClient( mMaxAck );
        }

        return 0;
    }

    

} // namespace socket
