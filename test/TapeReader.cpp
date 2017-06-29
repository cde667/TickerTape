
#include <thread>
#include <vector>
#include <mutex>
#include <sstream>

#include "TestTape.h"
#include "TestShm.h"

/*
 * Tape Reader
 *    This is the other piece of the tape reader / writer.  Here, we connect to an existing tape and
 *    then read the messages off of the tape.
 */
std::mutex g_display_mutex;

class MessageLogger
{
    std::thread::id mThrId;

  public:
    MessageLogger()
    {
        mThrId = std::this_thread::get_id();
    }

    ~MessageLogger()
    {}

    void process( const test::Message1& msg )
    {
        g_display_mutex.lock();
        std::cout << "Message Logger " << mThrId << " : Message " << msg.size() << std::endl;
        if (!msg.verify()) {
            std::cerr << "Error : Message failed to verify!" << std::endl;
        }
        g_display_mutex.unlock();
    }

    void process( const test::Message2& msg )
    {
        g_display_mutex.lock();
        std::cout << "Message Logger " << mThrId << " : Message " << msg.size() << std::endl;
        if (!msg.verify()) {
            std::cerr << "Error : Message failed to verify!" << std::endl;
        }
        g_display_mutex.unlock();
    }

    void process( const test::Message3& msg )
    {
        g_display_mutex.lock();
        std::cout << "Message Logger " << mThrId << " : Message " << msg.size() << std::endl;
        if (!msg.verify()) {
            std::cerr << "Error : Message failed to verify!" << std::endl;
        }
        g_display_mutex.unlock();
    }

    void process( const test::Message4& msg )
    {
        g_display_mutex.lock();
        std::cout << "Message Logger " << mThrId << " : Message " << msg.size() << std::endl;
        if (!msg.verify()) {
            std::cerr << "Error : Message failed to verify!" << std::endl;
        }
        g_display_mutex.unlock();
    }

    void process( const test::Message5& msg )
    {
        std::cout << "Message Logger " << mThrId << " : Message " << msg.size() << std::endl;
        if (!msg.verify()) {
            std::cerr << "Error : Message failed to verify!" << std::endl;
        }
    }
};


bool gDone = false;

void test_reader_thread( test_shm::Shmem* memory )
{
    MessageLogger logger;

    // put the tape structure over this...
    test::MessageTape *tape = new( memory->connect()) test::MessageTape();

    // Now, let's create my reader...
    test::MessageReader< MessageLogger > reader( *tape, logger );

    while ( !gDone ) {
        reader.poll();
    }

}

int main( int argc, char ** argv )
{
    // Attach to the shared memory segment...
    key_t shmKey = 0x12345;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-k") == 0) {
            std::stringstream ss;
            ss << std::hex << argv[++i];
            ss >> shmKey;
        }
        else if (strcmp(argv[i], "-K") == 0) {
            shmKey = atoi(argv[++i]);
        }
    }


    test_shm::Shmem *myMemory = new test_shm::Shmem( shmKey, true );

    // Create my readers...
    int nReaders = 1;
    std::vector< std::thread > threads;

    for (int i = 0; i < nReaders; i++) {
        threads.emplace_back( test_reader_thread, myMemory );
    }

    while(true) {
        usleep(100 * 1000);
        for (auto iter = threads.begin(); iter != threads.end(); ) {
            if (iter->joinable()) {
                iter->join();
                iter = threads.erase(iter);
            }
            else {
                ++iter;
            }
        }
    }

}
