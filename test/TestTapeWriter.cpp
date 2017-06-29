#include <iostream>
#include <vector>
#include <thread>
#include <cstring>
#include <atomic>

// For you windows people, my appologies...
#include <unistd.h>

#include "../detail/TapeWriter.h"

using namespace detail;

//
// Writing 1024 byte messages...
//
struct TapeBuffer
{
    struct {                    // A possible header for the stuff...
        uint8_t msgId;
        uint8_t d1;
        uint16_t d2;
        uint32_t d3;
    } mHeader;

    char mData[1024 - sizeof(mHeader)];

    TapeBuffer() {
        mHeader.msgId = 0xFF;
        mHeader.d1 = 0xFF;
        mHeader.d2 = 0xFFFF;
        mHeader.d3 = 0xFFFFFFFF;
        memset( mData, 0, sizeof(mData) );
    }

    template< typename T >
    void set( const T& data )
    {
        static_assert( (sizeof(T) < sizeof(mData)), "Error : Structure is too big in size." );

        // Zero out the header
        memset( &mHeader, 0, sizeof(mHeader) );

        // Copy the data in...
        memcpy( mData, &data, sizeof(T) );

        // Zero the rest of this...
        if (sizeof(T) != sizeof(mData)) {
           memset( mData + sizeof(T), 0, sizeof(mData) - sizeof(T) );
        }
    }

    void finalize( uint8_t MsgId ) 
    {
        mHeader.d3 = 0;
        mHeader.d2 = 0;
        mHeader.d1 = 0;
        mHeader.msgId = MsgId;
    }

    uint8_t getMsgId(void) const 
    {
        return mHeader.msgId;
    }

    template< typename T >
    const T& getData( void ) const
    {
        return * static_cast<const T*>( static_cast<const void*>(&mData[0]) );
    }
};

class TestTape 
{
    const int mEntries;
    std::atomic<size_t> mPosition;
    TapeBuffer *mData;

  public:
    TestTape(int nMessages) : mEntries(nMessages) { 
        mData = new TapeBuffer[mEntries]; 
    }

    ~TestTape(){}

    TapeBuffer& getBuffer( void )
    {
        return mData[ (++mPosition) % mEntries ];
    }

    const TapeBuffer& readBuffer( int which ) 
    {
        if (which > mEntries) 
            throw std::runtime_error("Out of range...");

        return mData[ which ];
    }
};

//
// What messages am I going to handle : I'm going to define
// a generic template which I can easily test...
//
template< size_t N >
class TestMessage 
{
    static_assert( N > sizeof(int), "Minnimum message size is sizeof(int).");
   
    int mThread;
    int mId;
    char data[ N ];

  public:
    TestMessage( int thread, int count ) 
    {
        mThread = thread;
        mId = count;

        int position = 0;
        while (position + sizeof(int) <= N) {
            int* writeTo = (int*)(void*)( &data[position] );
            *writeTo = count;
            position += sizeof(int);
        }
    }

    int thread(void) const { return mThread; }
    int id(void) const { return mId; }

    bool verify( int thread, int count ) const
    {
        if (mThread != thread || mId != count ) {
            std::cout << "Failed to verify header.  Expected (thread,count) of (" << thread << "," << count 
                      << "); got (" << mThread << "," << mId << ")\n";
            return false;
        }

        int position = 0;
        while (position + sizeof(int) <= N) {
            int* verify = (int*)(void*)( &data[position] );
            if ( *verify != count) {
                std::cerr << "Failed to verify data.  Expected " << count << ", found " << (*verify) << std::endl;
                return false;
            }

            position += sizeof(int);
        }
        return true;
    }
};

volatile bool start = false;

template< size_t N >
void writeTestMessage( TestTape* tape, int thread, int count)
{
    TestMessage<N> message(thread, count);

    auto& buffer = tape->getBuffer();
    buffer.set( message );
    buffer.finalize( thread );
}

template< size_t N>
bool verifyTestMessage( const TapeBuffer& buffer, int thread, int count )
{
    const TestMessage<N>& msg = buffer.template getData<TestMessage<N> >();
    return msg.verify( thread, count );
}

void thread_main( TestTape* tape, int thrId, int nMsgs, int sleepTime )
{
    std::cout << "Thread Main : " << thrId << ", Messages " << nMsgs << ", Sleep " << sleepTime << ".\n";

    // Wait until we are told to go...
    while (!start) 
        ;

    std::cout << "GO : " << thrId << std::endl;

    for (int i = 0; i < nMsgs; i++) {
        switch( thrId % 5) {
            case 0: writeTestMessage<64>(tape, thrId, i); break;
            case 1: writeTestMessage<128>(tape, thrId, i); break;
            case 2: writeTestMessage<77>(tape, thrId, i); break;
            case 3: writeTestMessage<480>(tape, thrId, i); break;
            case 4: writeTestMessage<163>(tape, thrId, i); break;
            default: throw std::runtime_error("WTF!?!?");
        }

        usleep(sleepTime);

        if ((i%10) == 9) {
            std::cout << "Thread " << thrId << " wrote message " << i << "\n";
        }
    }

    std::cout << "Thread " << thrId << " wrote " << nMsgs << std::endl;
}

int main( int argc, char** argv )
{
    int nThreads = 2;
    int msgsPerThread = 100;
    int sleepTime = 1000;

    for (int i = 1; i < argc; i++) {
        if ( !strcmp(argv[i], "-t") || !strcmp(argv[i], "-threads") ) {
            nThreads = atoi( argv[++i] );
        }
        else if ( !strcmp(argv[i], "-m") || !strcmp(argv[i], "-messages") ) {
            msgsPerThread = atoi( argv[++i] );
        }
        else if ( !strcmp(argv[i], "-s") || !strcmp(argv[i], "-sleep") ) {
            sleepTime = atoi( argv[++i] );
        }
    }

    int nMessages = nThreads * msgsPerThread;

    // Create the Tape class...
    TestTape* tape = new TestTape( 2 * nMessages );

    // Create the threads...
    std::vector< std::thread > producers(nThreads);

    for (int i = 0; i < nThreads; i++) {
        std::cout << "Creating thread " << i << std::endl;
        producers.emplace_back( thread_main, tape, i, msgsPerThread, sleepTime );
    }

    usleep(10 * 1000000 ); // 10 seconds before starting...

    // Start it...
    start = true;

    usleep( 10 * 1000000 ); // the join doens't work right in some cases. TBD.

    // Join all threads...
    try {
        for (auto iter = producers.begin(); iter != producers.end(); iter++) {
            if (iter->joinable()) 
                iter->join();
        }
    } catch( std::system_error &err ) {
        std::cerr << "Error : " << err.what() << std::endl;
    }

    // Verify the contents of the tape.  While I don't know the order, 
    int seenCount[nThreads];
    memset( seenCount, 0, sizeof(seenCount) );

    bool result = true;

    std::cout << "\nDoing verification...\n";

    for (int i = 1; i <= nMessages; i++) {
        auto& buffer = tape->readBuffer(i);
        uint8_t thread = buffer.getMsgId();         // Set to the Thread ID of the message

        if ( thread >= nThreads ) {
            std::cerr << "Error : Message corrupt at position " << i << std::endl;
            break;
        }

        int count = seenCount[thread]++;

        switch( thread % 5 ) {
            case 0: result &= verifyTestMessage<64>(buffer, thread, count); break;
            case 1: result &= verifyTestMessage<128>(buffer, thread, count); break;
            case 2: result &= verifyTestMessage<77>(buffer, thread, count); break;
            case 3: result &= verifyTestMessage<480>(buffer, thread, count); break;
            case 4: result &= verifyTestMessage<163>(buffer, thread, count); break;
            default: throw std::runtime_error("WTF!?!?");
        }

        if (result == false) {
            std::cout << "Error processing message " << i << " : verifyTestMessage() failed : count=" << count
                      << ", ID = " << count << "\n";
            break;
        }
    }

    for (int i = 0; i < nThreads; i++) {
        result &= (seenCount[i]==msgsPerThread);
        if (!result) { 
            std::cout << "Error processing counts at " << i << " : expected " << nMessages
                      << ", got " << seenCount[i] << std::endl;
        }
    }

    if (!result) {
        std::cerr << "TEST FAILED!" << std::endl;
    }
}

