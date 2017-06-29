#pragma once

#include <sys/time.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include "../DBG.h"
#include "SyncSocketClient.h"

namespace sockets
{

    SyncSocketClient::SyncSocketClient( int family, int type, int proto )
        : mSockFamily(family), mSockType(type), mSockProto(proto)
    { }        

    void SyncSocketClient::set( const std::string& str, int port )
    {
        // for now, just use inet_addr() on the address ... 
        in_addr_t inaddr = inet_addr( str.c_str() );
        if (inaddr == INADDR_NONE) {
            std::cerr << "Error : invalid internet address specified : " << str << std::endl;
            throw std::runtime_error("Invalid internet address");
        }

        sockaddr_in& sin = * (sockaddr_in*)(void*)(&mAddr);
        sin.sin_port = htons( (short)(port) );
        sin.sin_addr.s_addr = inaddr;
        sin.sin_family = mSockFamily;
    }

    bool SyncSocketClient::connect(void) 
    {
        mFd = socket( mSockFamily, mSockType, mSockProto );
        if (mFd < 0) {
            std::cerr << "Error : socket : Could not open socket : " << strerror(errno) << std::endl;
            return false;
        }

        if ( ::connect( mFd, (sockaddr*) &mAddr, sizeof(mAddr) ) != 0) {
            std::cerr << "Could not connect : " << strerror(errno) << "\n";
            return false;
        }

        // Set non-blocking
        // TODO : set flags function in base class
        int flags = fcntl(mFd, F_GETFL, 0);
        if (flags >=0) {
            flags = (flags|O_NONBLOCK);
            if ( fcntl(mFd, F_SETFL, flags) < 0) {
                std::cerr << "Failed to set nonblocking flag : " << strerror(errno) << "\n";
            }
        }
        else {
            std::cerr << "Could not get flags : " << strerror(errno) << "\n";
        }

        return true;
    }

    template< typename TapeSync >
    size_t SyncSocketClient::parse( TapeSync& sync, void* vbuffer, size_t vbytes )
    {
        size_t retV = 0; // number of bytes consumed...
        char* buffer = (char*) vbuffer;
        size_t bytes = vbytes;
        bool processed = false;

        do 
        {
            processed = false;
            SocketMsgs::Header& h = *(SocketMsgs::Header*)(buffer);

            std::cerr << "PROCESS : retV = " << retV << ".  msgType " << h.msgType << std::endl;

            switch( h.msgType )
            {
                case  SocketMsgs::eHeartbeat:
                    if (bytes >= sizeof(SocketMsgs::Heartbeat )) {
                        retV += sizeof( SocketMsgs::Heartbeat );
                        processed = true;
                    }
                    break;

                case  SocketMsgs::eKeepAlive:
                    if ( bytes >= sizeof( SocketMsgs::KeepAlive )) {
                        sendHeartbeat();
                        retV += sizeof( SocketMsgs::KeepAlive );
                        processed = true;
                    }
                    break;

                case  SocketMsgs::eTapeBuffer:
                    if (bytes >= sizeof(SocketMsgs::SendBuffer)) {
                        SocketMsgs::SendBuffer& b = * (SocketMsgs::SendBuffer*)(buffer);
                        size_t msgSize = b.mBytes;
                        uint64_t seq = b.mSequence;
                        
                        if (bytes >= sizeof( SocketMsgs::SendBuffer ) + msgSize) {
                            void* data = (void*) ( ((char*) buffer) + sizeof(SocketMsgs::SendBuffer) );
                            mNextAck = sync.processBuffer( seq, data, msgSize );
                            retV += sizeof( SocketMsgs::SendBuffer ) + msgSize;
                            processed = true;
                        }
                    }
                    break;

                case  SocketMsgs::eAck:
                    break;

                case  SocketMsgs::eResendRequest:
                    break;

                default:
                    throw std::runtime_error("Corrupt!");
            }

            // next pass thru : move to the next message.
            buffer = (char*) vbuffer + retV;
            bytes = vbytes - retV;
        } while ( processed && bytes > 0 );

        return retV;
    }

    // Send an ACK back to the server for all the packets we just processed
    void SyncSocketClient::sendAck()
    {
        if ( mNextAck - 1 != mLastAck) {
            SocketMsgs::Ack ack( mLastAck+1, mNextAck - 1); // changed : now -1
            send( mFd, &ack, sizeof(ack), O_NONBLOCK );
            mLastAck = mNextAck;
            _DBG_(std::cout << "DBG : Sending acks to " << mNextAck << std::endl;);
        }
        else {
            _DBG_(std::cout << "No ACK : " << mNextAck << ".  " << mLastAck << "\n";);
        }
    }

    void SyncSocketClient::sendHeartbeat()
    {
        SocketMsgs::Heartbeat h;
        ssize_t result = send( mFd, &h, sizeof(h), O_NONBLOCK );
        if (result != sizeof(h)) {
            std::cerr << "Failed to write heartbeat message..." << std::endl;
        }
    }

    void SyncSocketClient::sendKeepAlive()
    {
        SocketMsgs::KeepAlive ka;
        size_t result = send( mFd, &ka, sizeof(ka), O_NONBLOCK );
        if (result != sizeof(ka)) {
            std::cerr << "Failed to write heartbeat message..." << std::endl;
        }
    }

    static const size_t BUFFER_SIZE = 2048;
    char mBuffer[ BUFFER_SIZE ];
    ssize_t mBytes = 0;

    template< typename TapeSync >
    size_t SyncSocketClient::read( TapeSync& sync )
    {
        //static const size_t BUFFER_SIZE = 2048;
        //char buffer[ BUFFER_SIZE ];
        ssize_t bytes = 0;
        int count = 0;
          
        while ((bytes = recv( mFd, mBuffer + mBytes, BUFFER_SIZE - mBytes, O_NONBLOCK )) > 0) {
            _DBG_(std::cout << "DBG : BYTES : " << bytes << ".  mBytes " << mBytes << "\n";);
            size_t consumed = parse( sync, mBuffer, bytes + mBytes );

            if (consumed < bytes + mBytes) {
                memmove( mBuffer, mBuffer + consumed, bytes + mBytes - consumed );
                mBytes = bytes + mBytes - consumed;
            }
            else  {
                mBytes = 0;
            }

            ++count;
        }

        if (bytes < 0) {
            if (errno == EAGAIN ) { 
                
            }
            else {
                // Close the socket and re-connect.
                // TODO : this
            }
        }
        else if (bytes == 0 && mSockFamily == SOCK_STREAM) {
            _DBG_(std::cout << "ZERO BYTE READ!\n";);

            // Close socket and re-connect
            // TODO : this
        }
        
        // Send ACK back to the server
        if ( count ) {
            sendAck();
            gettimeofday( &mLastRecv, nullptr );
        }

        return 0;
    }

} // namespace socket
