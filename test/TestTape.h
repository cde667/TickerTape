#pragma once

#include <cstddef>
#include "../detail/MessageSize.h"
#include "../TapeWriter.h"
#include "../TapeReader.h"
#include "../MemoryTape.h"

/*
 * Test Tape
 *    This creates a series of messages (structures) that we will be sending across our 
 *    tape.  Once the messages are created, we then use the templates above to create
 *    a tape reader and tape writer for the given messages!
 */

namespace test
{

    template<size_t msgSize>
    struct Message
    {
        char data[4 * msgSize];
        
        Message(void) 
        {
            for (int i = 0; i < 4 * msgSize; i++) {
                data[i] = 'A' + ((msgSize+i)%64);
            }
        }

        ~Message()
        {}

        bool verify() const
        {
            bool retV = true;

            for (int i = 0; i < 4 * msgSize; i++) {
                char c = 'A' + ((msgSize+i)%64);
                retV &= (c == data[i]);
            }

            return retV;
        }

        size_t size(void) const
        {
            return msgSize;
        }
    };

   
    //
    // The messages I will send in this protocol...
    //
    using Message1 = Message<32>;
    using Message2 = Message<122>;
    using Message3 = Message<62>;
    using Message4 = Message<7>;
    using Message5 = Message<382>;

    //
    // Define the tape...
    //
    using MessageTape = Tape< detail::MessageSize<Message1, Message2, Message3, Message4, Message5>::msgSize>;
 

    //
    // Define my Writer
    //
    using MessageWriter = TapeWriter< MessageTape,
                                      Message1, Message2, Message3, Message4, Message5 >;

    //
    // Define my Reader
    //
    template< typename TProcessor >
    using MessageReader = TapeReader< MessageTape, TProcessor,  
                                      Message1, Message2, Message3, Message4, Message5 >;

}

