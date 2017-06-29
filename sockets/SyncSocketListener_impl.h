#pragma once

#include <string>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace sockets
{

    //
    // Listen for inbound connections
    //
    SyncSocketListener::SyncSocketListener( int family, int type, int proto )
        : mSockFamily(family), mSockType(type), mSockProto(proto)
    { }     

    void SyncSocketListener::set( const std::string& addr, int port )
    {
        if (mSockFamily != PF_INET) {
            throw "Only Inet sockets supported currently...";
        }

        // for now, just use inet_addr() on the address ...
        in_addr_t inaddr = inet_addr( addr.c_str() );
        if (inaddr == INADDR_NONE) {
            std::cerr << "Error : invalid internet address specified : " << addr << std::endl;
            throw std::runtime_error("Invalid internet address");
        }

        sockaddr_in& sin = * (sockaddr_in*)(void*)(&mAddr);
        sin.sin_port = htons( (short)(port) );
        sin.sin_addr.s_addr = inaddr;
        sin.sin_family = mSockFamily;
    }

    // Connect opens up a socket to listen on...
    bool SyncSocketListener::connect(void) 
    {
        mFd = socket( mSockFamily, mSockType, mSockProto );
        if (mFd < 0) {
            sockaddr_in& sin = * (sockaddr_in*)(void*)(&mAddr);
            std::cerr << "Error : socket : Could not open socket to " << inet_ntoa( sin.sin_addr ) << ":" << htons(sin.sin_port) << "\n";
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

        if (bind( mFd, &mAddr, sizeof(mAddr) ) < 0) {
            sockaddr_in& sin = * (sockaddr_in*)(void*)(&mAddr);
            std::cerr << "Error : bind : could not bind to " << inet_ntoa( sin.sin_addr) << ":" << htons(sin.sin_port) << "\n";
            close(mFd);
            return false;
        }

        if ( listen( mFd, 10 ) != 0) {
            sockaddr_in& sin = * (sockaddr_in*)(void*)(&mAddr);
            std::cerr << "Error : listen : could not listen to " << inet_ntoa( sin.sin_addr) << ":" << htons(sin.sin_port) << "\n";
            close(mFd);
            return false;
        }

        return true;
    }

    // read : create a new inbound connection
    template< typename TSyncServer >
    bool SyncSocketListener::read( TSyncServer& sync )
    {
        struct sockaddr addr;
        socklen_t addrLen = sizeof(addr);
        int fd = accept( mFd, &addr, &addrLen );
        if (fd < 0) {
            if (errno == EAGAIN) {
                return false;
            }
            else {
                // Accept failed... 
                // TODO : Clean this up.
                throw std::runtime_error("Accept failed");
            }
        }

        SyncSocketServer *sock = 
            new SyncSocketServer( mSockFamily, mSockType, mSockProto, fd, addr );

        sync.addSync( *sock );

        // Log it
        if ( mSockFamily == PF_INET) {
            struct sockaddr_in& sin = * (sockaddr_in*)(void*)(&addr);

            char buffer[128];
            std::cout << "LOG : New sync connection from " 
                  << inet_ntoa( sin.sin_addr )
                  << ":" << ntohs( sin.sin_port ) << std::endl;
        }
        else {
            std::cout << "New Connection : Log it here..." << std::endl;
        }
        return true;
    }

} // namespace socket

