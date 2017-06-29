#pragma once

// TODO : break up, move bunch into the detail directory

#include <iostream>
#include <unistd.h>
#include <cstring>
#include <atomic>

#include "TapeHeaderMessage.h" // TODO : Add in a verification somewhere... After I get the SFINAE stuff working!
#include "detail/Types.h"
#include "detail/CacheLine.h"

namespace detail
{

    //
    // Tape Entry Header
    //    Contains the vital data for a cell inside of the tape.  Currently
    //    I'm just sticking in the message type flag, but could add in other
    //    things in the future... (timestamp? sequence?)
    //
    // TODO :
    //    1) Expand message type into 2 uint8_ts.  First being a protocol id, second being the msg
    //    2) Add in a size argument so I can quickly send the correct number of bytes in the data
    //    3) Add in an ID tag to flag who wrote it
    // All these get set in the finalize() and/or sync() methods...
    //
    struct TapeEntryHeader
    {
        TMessageType mRawMsgType;      // Raw message type : used by the sync to determine if ready 
        TMessageType mMsgType;         // Message type : used by NON-sync to determine if readable.
        char    _padding[CacheLineSize - 2*sizeof(TMessageType)];     // 64 bytes : this should be a cache line to avoid invalidations while writing.
                                                               // It may be a bit wasteful, but ... well, we will see.
        TapeEntryHeader()
        {}

        ~TapeEntryHeader()      // Don't actually destroy anything : other things could be using it.
        {}

        inline void clear(void) 
        {
            memset(this, 0, sizeof(TapeEntryHeader));
        }

        inline void finalize( TMessageType msgType )
        {
            std::atomic_thread_fence( std::memory_order_acq_rel );
            mRawMsgType = msgType;
        }

        inline void sync()
        {
            //std::atomic_thread_fence( std::memory_order_acq_rel );
            //do {
                mMsgType = mRawMsgType;
            //} while (mMsgType == 0);
            
        }

        inline TMessageType poll(void) const 
        {
            return (mMsgType);
        }

        inline TMessageType raw_poll(void) const
        {
            return (mRawMsgType);
        }
    };

    //
    // Tape Entry
    //   One single cell in the tape.  This contains a header (above) and
    //   the raw data array.
    //
    template< size_t DataSize >
    class TapeEntry 
    {
        TapeEntryHeader mHeader;
        uint8_t         mData[DataSize];

      public:
        TapeEntry()
        {}

        ~TapeEntry()
        {}              // Don't actually destruct anthing : others may be using it!

        inline size_t size(void) const 
        {
            return DataSize + sizeof(mHeader);
        }

        inline void clear(void)
        {
            mHeader.clear();
            memset(mData, 0, DataSize);
        }

        template< typename T >
        inline void set( const T& data )
        {
            static_assert( (sizeof(T) < sizeof(mData)), "Error : Structure is too big in size." );

            // DBG : Zero out the header (it should already be zero'd)
            memset( &mHeader, 0, sizeof(mHeader) );

            // Copy the data in...
            memcpy( &mData[0], &data, sizeof(T) );

            // DBG : Zero the rest of this...
            if (sizeof(T) != sizeof(mData)) {
               memset( mData + sizeof(T), 0, sizeof(mData) - sizeof(T) );
            }
        }

        inline void finalize( uint8_t msgType )
        {
            mHeader.finalize( msgType );
        }

        inline void sync()
        {
            mHeader.sync();
        }

        inline TMessageType poll(void) const
        {
            return mHeader.poll();
        }

        inline TMessageType raw_poll(void) const
        {
            return mHeader.raw_poll();
        }

        template< typename T >
        inline const T& get( void ) const
        {
            return *(static_cast<const T*>( 
                        static_cast<const void*>(&mData[0]) 
                     ));
        }

        template< typename T >
        inline T& get( T& data ) const
        {
            memcpy( &data, &mData[0], sizeof(T) );
            return data;
        }

        inline void* writeRaw( size_t bytes, const void* data )
        {
            // Write a raw cell into our tape.
            if (bytes > DataSize + sizeof(mHeader)) {
                std::cerr << "Error : Attempt to write packet that is too big.\n";
                return nullptr;
            }
        
            memcpy( mData, ((const char*)data) + sizeof(mHeader), bytes - sizeof(mHeader) );

            // Write the header...
            const TapeEntryHeader& hdr = * (const TapeEntryHeader*)(data);
            mHeader.finalize( hdr.mRawMsgType );
            return (void*) mData;
        }
    };

    //
    // Information that we need for any given shared memory tape...
    //
    struct TapeHeader
    {
        using TAPE_IDENTIFIER = int;            // for now.

        size_t              nEntries;   // Entries in this tape.  This is const but cannot be due to the shared memory we want to use.
        std::atomic<size_t> mPosition;  // position where we are WRITING to
        TAPE_IDENTIFIER     mNextTape;  // So we can allocate smaller tapes.  The synchronizer will check this tape periodically
                                        // to see if there is data in it.  If there is data in this tape, it will allocate a new
                                        // tape and this should indicate how to get to it.  When we hit the end of the tape, we
                                        // can create a new tape using this identifier.  This is a future thing.  This note should
                                        // allow me to remeber how to do this.  Solves several of our problems and gives us the
                                        // ability to free and/or reuse tapes as they are completed for, in theory, infinite running.

        static const size_t MySize = sizeof(size_t) + sizeof(std::atomic<size_t>) + sizeof(TAPE_IDENTIFIER);
        char _padding[ 64 - MySize];

        TapeHeader()
        {
            // The correct thing to do here is to NOT initialize anything, as we are attaching
            // to an existing shared memory segment.  The values are already set!
            // TODO : Figure out how to swallow the warnings gracefully
        }
        
        TapeHeader(size_t entries) 
            : nEntries(entries)
            , mPosition(0)
            , mNextTape(0)
        { }

        ~TapeHeader()
        {} // Don't destruct : others may be using it.

        size_t next(void) 
        {
            size_t retV = mPosition++;
            if (retV >= nEntries) {
                // This should trigger moving to the next tape using the mNextTape stuff above.  For now, we just
                // throw an error and will handle it later.
                --mPosition;
                throw std::runtime_error( "Out of buffers." );
            }

            return retV;
        }

        void position( uint64_t pos )
        {
            if (pos > mPosition) {
                mPosition = pos;
            }
        }
    };
}


//
// A simple tape header message...
//

template< size_t DataSize >
class Tape
{
  public:
    static constexpr size_t CellDataSize = DataSize + 
                                           sizeof(detail::TapeEntryHeader) + 
                                           ( detail::CacheLineSize - ((DataSize + sizeof(detail::TapeEntryHeader))) % detail::CacheLineSize );

    static constexpr size_t MsgWireSize = sizeof( detail::TapeEntry<CellDataSize> );

    using TTapeEntry = detail::TapeEntry<CellDataSize>;

  private:
    detail::TapeHeader   mHeader;
    TTapeEntry           mEntries[0];               // Hack : stops i
    
        
  public:
    using TapeEntry = detail::TapeEntry<CellDataSize>;

    // May make the constructors private and hide in a factory somewhere....

    // The tape is already created : we are attaching to it.
    Tape()
        : mHeader()
    { 
        int count = 0;
#if 0
        TapeHeaderMessage found;

        while( count < 100 ) {
            // Verify the first message 
            mEntries[0].get( found );

            if (found.verify()) {
                break;
            }

            usleep(1000);
            ++count;
        }

        if (count > 99 ) {
            std::cerr << "Error : Failed to verify the initial message on the tape.  Expected "
                      << found.expected() << ", got " << found.txt() << std::endl;

            throw std::runtime_error("Version mis-match on the memroy stuff...");
        }
#endif
    }

    // We are CREATING the tape
    Tape( size_t bytes )
        : mHeader( (bytes - sizeof(mHeader)) / sizeof(TTapeEntry) )
    {
        for (int i = 0; i < mHeader.nEntries; i++) {
            mEntries[i].clear();
        }

#if 0
        // TODO : Build up tape header...

        mEntries[0].set( TapeHeaderMessage() );
        mEntries[0].finalize( 0xFFFF );

        std::cout << "Tape created.  Wrote start message." << std::endl;

        TapeHeaderMessage found;
        mEntries[0].get(found);
        if (!found.verify()) {
            std::cerr << "Error : Tape did not verify." << std::endl;
            throw std::runtime_error("Tape error : Tape did not verify.");
        }

        std::cout << "Tape verified\n";
#endif
    }


    // 
    // Destructor : we don't destroy anything; other things might be using it...
    //
    ~Tape()
    {}

    //
    // Write Functions
    //
    TapeEntry& getBuffer(void)
    {
        return mEntries[ mHeader.next() ];
    }

    inline void position(size_t pos) {
        mHeader.position(pos);
    }

    //
    // For the case of the synchronizer....
    //   TODO : Make private; friend with the synchronizer template class
    //
    TapeEntry& getBuffer( uint64_t position )
    {
        return mEntries[ position ];
    }

    //
    // See what data is available (and what type it is)
    //   ... message type 0 is EMPTY!
    //
    inline const detail::TMessageType poll( size_t position )
    {
        return mEntries[ position ].poll();
    }

    inline const detail::TMessageType raw_poll( size_t position )
    {
        return mEntries[ position ].raw_poll();
    }

    inline void sync( size_t position )
    {
        mEntries[ position ].sync();
    }

    //
    // Read a raw buffer from the system
    //
    const TTapeEntry& readBuffer( size_t position )
    {
        if ( position > mHeader.nEntries ) {
            throw std::range_error( "Attempting to read beyond end of buffer.");
        }

        if ( mEntries[ position ].poll()) {    // DBG
            return mEntries[ position ];       // Tape buffer
        }

        throw std::runtime_error( "Attempting to read from buffer that is still zero'd.");
    }

    const TTapeEntry& rawReadBuffer( size_t position )
    {
        if ( position > mHeader.nEntries ) {
            throw std::range_error( "Attempting to read beyond end of buffer.");
        }

        if ( mEntries[ position ].raw_poll()) {    // DBG
            return mEntries[ position ];       // Tape buffer
        }

        throw std::runtime_error( "Attempting to read from buffer that is still zero'd.");
    }


    //
    // Read a raw object...
    template< typename T >
    const T& readObject( size_t position )
    {
        if ( position > mHeader.nEntries ) {
            throw std::range_error( "Attempting to read beyond end of buffer.");
        }

        if ( mEntries[ position ].poll()) {
            return mEntries[ position ].template get<T>();
        }

        throw std::runtime_error( "Attempting to read from buffer that is still zero'd.");
    }

    //
    // Copy object into given structure...
    //
    template< typename T >
    T& readBuffer( size_t position, T& data )
    {
        if ( position > mHeader.nEntries ) {
            throw std::range_error( "Attempting to read beyond end of buffer.");
        }

        if ( mEntries[ position ].poll()) {
            return mEntries[ position ].get( data );
        }

        throw std::runtime_error( "Attempting to read from buffer that is still zero'd.");
    }


#if 0
    // Later ... ?

    // Placement new : use the existing block of memory and put this
    Tape* operator new(void* data, size_t bytes)
    {
        
    }

    // Placement new : create a block pointed to by key_t of the given number of bytes.
    // In this case, we are zeroing out the entire array as we are the CREATOR.
    Tape* operator new( key_t shmkey, size_t bytes )
    {

    }
    
    // Placement new : get the block at shmkey and use it to set the rest of the data fields
    // in the tape.
    Tape* operator new( key_t shmkey )
    {

    }
#endif


};
