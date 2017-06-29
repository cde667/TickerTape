#include <iostream>
#include <thread>
#include <vector>

#include "TestTape.h"
#include "TestShm.h"

/*
 * Tape writer 
 *    This is a simple program to create a tape and a sereies of writers for testing.  We create the
 *    tape in a shared memory buffer.  We then create the test writers in each of their own threads
 *    and connect to the shared memory buffer...
 */
static bool goForIt = false;
static std::atomic<unsigned int> msgCount(0);

void test_writer_thread( test_shm::Shmem* memory, int nMsgs, uint64_t sleepTime )
{
    std::cout << "Tape Writer : starting thread..." << std::endl;
    usleep( 1 * 1000 * 1000 );
    int i;

    try {

    test::MessageTape *tape = new( memory->connect() ) test::MessageTape( );

    // Create my writer...
    test::MessageWriter writer( *tape );

    while ( !goForIt )
        ;
 
    for ( i = 0; i < nMsgs; i++ ) {
        
        switch( i % 5 ) {
            case 0:
            {
                test::Message1 msg;
                writer.publish(msg);
            }
            break;

            case 1:
            {
                test::Message2 msg;
                writer.publish(msg);
            }
            break;

            case 2:
            {
                test::Message3 msg;
                writer.publish(msg);
            }
            break;

            case 3:
            {
                test::Message4 msg;
                writer.publish(msg);
            }
            break;

            case 4:
            {
                test::Message5 msg;
                writer.publish(msg);
            }
            break;
        }

        ++msgCount;
        usleep( sleepTime );
    }

    } catch( std::runtime_error &e ) {
        std::cerr << msgCount.load() << " : " << i << " / " << nMsgs << " : Caught Runtime error : " << e.what() << std::endl;
    }

    // Release the memory...
    memory->release();

}


int main( int argc, char ** argv )
{
    // The shared memory segment should be created by the Synchronizer only
    key_t shmKey = 0x12345;
    test_shm::Shmem* myMemory = new test_shm::Shmem( shmKey, false);

    int nWriters = 3;
    uint64_t sleepTime = 3 * 1000;   //  3 milliseconds for starters...
    std::vector< std::thread > threads;
    int nMsgsToSend = (myMemory->bytes() / test::MessageTape::MsgWireSize) / nWriters - 10;
    for (int i = 0; i < nWriters; i++) {
        threads.emplace_back( test_writer_thread, myMemory, nMsgsToSend, sleepTime);
    }

    std::cout << "Start Memory Readers.  Then press RETURN to start...";
    getchar();

    //
    // Ok, let's run...
    //
    goForIt = true;
    while (!threads.empty()) {
        usleep(100 * 1000);

        for (auto iter = threads.begin(); iter != threads.end(); ) {
            if ( iter->joinable()) {
                iter->join();
                iter = threads.erase(iter);
            }
            else {
                ++iter;
            }
        }
    }
    
}

