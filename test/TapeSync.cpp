
#include <vector>

#include "../MemoryTape.h"
#include "../sockets/SocketMsgs.h"
#include "../sockets/SyncSocketClient.h"
#include "../sockets/SyncSocketServer.h"
#include "../sockets/SyncSocketListener.h"
#include "../TapeSync.h"

#include "../sockets/SyncSocketListener_impl.h"
#include "../sockets/SyncSocketClient_impl.h"
#include "../sockets/SyncSocketServer_impl.h"
#include "../TapeSync_impl.h"

#include "TestShm.h"
#include "TestTape.h"

using Sync = TapeSync< test::MessageTape, sockets::SyncSocketClient, sockets::SyncSocketListener, sockets::SyncSocketServer >;

int main( int argc, char ** argv )
{
    bool isPrimary = false;
    bool isClient = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            isPrimary = true;
        }
        else if (strcmp(argv[i], "-c") == 0) {
            isClient = true;
        }
    }

    //
    // Step 1 : Create my tape
    //   New thing going forward : only the SYNC process should create the tape.
    //
    int shmKey = (isPrimary ? 0x12345 : 0x12346);
    size_t nBytes = 1024 * 1024;   // For testing, we will use 64 megabytes of memory.  
    test_shm::Shmem* myMemory = new test_shm::Shmem( shmKey, nBytes, 0777 );

    test::MessageTape *tape = new( myMemory->connect() ) test::MessageTape( myMemory->bytes() );

    //
    // Step 2: Create the Sync'er
    //
    Sync* sync = new Sync( *tape, isPrimary );

    //
    // Step 3 : Create some sockets...stick them in the Sync.
    //
    std::vector< sockets::SyncSocketListener* > listeners;
    std::vector< sockets::SyncSocketClient* > clients;
        
    if (isPrimary) {
        sockets::SyncSocketListener* listener = new sockets::SyncSocketListener( PF_INET, SOCK_STREAM, IPPROTO_TCP );
        listener->set( "127.0.0.1", 34567 );
        listener->connect();
        listeners.push_back( listener );

        sync->addSync( *listener );
    }

    if (isClient) {
        sockets::SyncSocketClient* client = new sockets::SyncSocketClient( PF_INET, SOCK_STREAM, IPPROTO_TCP );
        client->set( "127.0.0.1", 34567 );
        client->connect();
        clients.push_back( client );

        sync->addSync( *client );
    }

    while (true) {
        sync->poll();
    }
}
