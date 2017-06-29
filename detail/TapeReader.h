#pragma once

#include <iostream>
#include <type_traits>
#include <array>
#include "../DBG.h"
#include "Types.h"

namespace detail
{
    // TODO : 
    // SFINAE Magic to determine if we have a function in our handler.  If we do not, we can put a dummy function in
    // automagically...  Stolen from stack overflow
    template< typename C, typename F, typename = void>
    struct is_call_possible : public std::false_type {};

    template < typename C, typename R, typename TMsgType >
    struct is_call_possible< C, R(TMsgType),
        typename std::enable_if< 
            std::is_same<R, void>::value ||
            std::is_convertible< decltype(
                    std::declval<C>().process( std::declval<TMsgType>())
            ), R>::value
         >::type
     > : public std::true_type{};

    // Read a single message of type TMsgType and call the correct processing function in the processor...
    template< TMessageType MsgId, 
              typename TTape, typename TProcess, typename TMsgType >
    class ReadTapeMsg
    {
        // Could add some other function signatures into this as well, i.e., 
        // ones that return an int ...

      public:
        static 
        void read( size_t position, TTape& tape, TProcess& processor )
        {
            auto& msg = tape.readBuffer(position);

            // Sanity checking...
            if ( msg.poll() != MsgId ) {
                throw std::runtime_error("Error : in wrong function!");
            }

            const TMsgType& data = msg.template get<TMsgType>();
            processor.process( data );
        }

#if 0
        // TODO : use some SFINAE crap to get this to work nicely.  For now, force all the
        // things to be defined.
        static // think i need a template< typename T = TProcess > here.
        std::enable_if_t< !is_call_possible< T, void, TMsgType>::value, bool >
        read( size_t position, TTape& tape, TProcess& processor )
        {
            // the processor does not support this signature.  
            // For now, just skip over it.

            // XXX ??? Error ??? Warning ???
            return true;
        }
#endif

    };

    //
    // The variadic template that puts all of these things together, creating all the read functions for the
    // right messages.  
    //
    template<  TMessageType MsgId, typename TTape, typename TProcess, typename... TTypes  >
    class TapeReader;

    template< TMessageType MsgId, typename TTape, typename TProcess, typename TMsgType, typename... TTypes >
    class TapeReader< MsgId, TTape, TProcess, TMsgType, TTypes...>
        : public TapeReader< MsgId + 1, TTape, TProcess, TTypes...>
        , public ReadTapeMsg< MsgId, TTape, TProcess, TMsgType >
    {

        using ReadTapeMsg<MsgId, TTape, TProcess, TMsgType>::read;

      protected:
        using TapeReader< MsgId + 1, TTape, TProcess, TTypes...>::poll;
        using TapeReader< MsgId + 1, TTape, TProcess, TTypes...>::registerHandler;

      public:
        TapeReader( TTape& tape, TProcess& process )
            : TapeReader< MsgId+1, TTape, TProcess, TTypes...>(tape, process)
            , ReadTapeMsg< MsgId, TTape, TProcess, TMsgType >()
        {
            // Store the handler for later use.
            //registerHandler( MsgId,  ReadTapeMsg<MsgId, TTape, TProcess, TMsgType>::read );
            _DBG_( std::cout << "Registering handler " << MsgId << "\n" );
            registerHandler( MsgId, read );
        }

    };

    template< TMessageType MsgId, typename TTape, typename TProcess >
    class TapeReader< MsgId, TTape, TProcess >
    {
        using TFcnPtr = void (*)(size_t, TTape&, TProcess&);

        // A default function to put into our table...
        static void nullFcn(size_t, TTape&, TProcess& )
        { }

        // Here, I have my maximum message ID.  I can create an array of some sort of pointers allowing me to 
        // jump to the correct function quickly and efficiently...  
        TTape& mTape;
        TProcess& mProcess;

        TFcnPtr mOffsetTable[MsgId];            // std::array< TFcnPtr, MsgId> mOffsetTable;
        size_t mPosition;

      public:
        TapeReader( TTape& tape, TProcess& process )
            : mTape(tape), mProcess(process), mPosition(0)
        { 
            for (int i = 0; i < MsgId; i++) {
                mOffsetTable[i] = nullFcn;
            }
        }

        void registerHandler( TMessageType msgId, TFcnPtr ptr )
        {
            if (msgId >= MsgId) {
                std::cerr << "Beyond the end of the array!!" << std::endl;
                throw std::runtime_error("Blaah");
            }

            mOffsetTable[ msgId ] = ptr;

            // DEBUG
            if (mPosition != 0) {
                std::cerr << "mPosition has changed -- " << msgId << std::endl;
            }
        }

        int poll()
        {
            int count = 0;

            TMessageType id;
            while( (id = mTape.poll(mPosition)) != 0 ) {
                mOffsetTable[id]( mPosition, mTape, mProcess );
                ++mPosition;
                ++count;
            }

            return count;
        } 

    };

}

