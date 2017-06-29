#pragma once

#include <cstdint>
#include <atomic>

namespace detail
{

    // ------------ Write Tape Message ----------
    // Write a single message to a given tape...
    // Note : extra fields hanging around currently.  Need to figure out what we want in our headers
    //        at some point. (XX YY ZZ may all vanish...)
    template< uint8_t MsgId, typename TTape, typename TMsgType >
    class WriteTapeMsg
    {
      public:
        void publish( TTape& tape, const TMsgType& msg )
        {
            auto& buffer = tape.getBuffer();
            buffer.set( msg );
            buffer.finalize( MsgId );       // Whatever header data we pass into the structure
        }
    };


    // ----------- WRITER ----------
    template<  uint8_t MsgId, typename TTape, typename... TTypes >
    class TapeWriter;

    template< uint8_t MsgId, typename TTape, typename TMsgType, typename... TTypes >
    class TapeWriter<MsgId, TTape, TMsgType, TTypes...>
        : public WriteTapeMsg<MsgId, TTape, TMsgType>
        , public TapeWriter<MsgId+1, TTape, TTypes...>
    {
        using TWriter = WriteTapeMsg<MsgId, TTape, TMsgType>;
        using TNext = TapeWriter<MsgId+1, TTape, TTypes...>;

      public:
        using TWriter::publish;
        using TNext::publish;

    };

    template<  uint8_t MsgId, typename TTape, typename TMsgType >
    class TapeWriter< MsgId, TTape, TMsgType >
        : public WriteTapeMsg< MsgId, TTape, TMsgType >
    {
         using TWriter = WriteTapeMsg<MsgId, TTape, TMsgType>;

       public:
         using TWriter::publish;
    };

};
