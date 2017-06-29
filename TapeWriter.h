#pragma once

#include <cstdint>
#include "detail/TapeWriter.h"

//
// TTape : a template that takes the messages and automagically figures out the 
// message size and creates the correct writer.  In the event of something like
// TCP, the size can be ignored.  But for shared memory, it can be useful to 
// maximize our data usage.
//
template< typename TTape, typename... TMsgTypes >
class TapeWriter
    : public detail::TapeWriter< 1,TTape, TMsgTypes... >
{
    using TWriter = detail::TapeWriter< 1, TTape, TMsgTypes... >;

  private:
    TTape& mTape;

  public:
    TapeWriter( TTape& tape ) 
        : mTape(tape)
    {}

    // Forwarder...
    template< typename T >
    inline void publish( const T& msg ) 
    {
        this->TWriter::publish( mTape, msg );
    }
};
