
#pragma push(pack, 1)

#include "DBG.h"

//
// TODO : size_t and uint64_t are used for sequence numbers.  Either standardize on uint64_t across all
// things --or-- create a TSequenceNumber and use that.  (preferably the later)
//

namespace detail
{

    template< typename TSync, typename... TSyncs >
    void Sync<TSync, TSyncs...>::doSync( uint64_t seq, const void* buffer, size_t size )
    {
        for( auto iter = mSyncs.begin(); iter != mSyncs.end(); iter++) {
            (*iter)->sendBuffer( seq, buffer, size );
        }

        Sync< TSyncs...>::doSync(seq, buffer, size);
    }

    template< typename TSync, typename... TSyncs>
    template< typename TSynchronizer>
    void Sync<TSync, TSyncs...>::read( TSynchronizer& sync )
    {
        // Loop thru all synchronizers of this type
        for( auto iter = mSyncs.begin(); iter != mSyncs.end(); iter++) {
            (*iter)->read(sync);
        }

        Sync< TSyncs...>::read( sync );
    }
}


template< typename TTape, typename... TSyncs>
void TapeSync<TTape, TSyncs...>::poll( )
{
    // Check our tape for data.  If we have data, we send it
    // to the next level...
    while ( mTape.raw_poll( mPosition ) ) {
        auto& entry = mTape.rawReadBuffer( mPosition );
        detail::Sync<TSyncs...>::doSync( mPosition, &entry, entry.size() );
        ++mPosition;
    }
        
    // Check our Syncers for ACKs, new connections, ...
    detail::Sync<TSyncs...>::read( *this );
}

//
// Process the ACKs coming back
//
template< typename TTape, typename... TSyncs >
void TapeSync<TTape, TSyncs...>::processClient( uint64_t seq )
{
    // If I am a master, I should get responses only
    if (isPrimary && seq >= mMaxAck) {
        // this client has ACK'd up to this number : mark them as such...
        for ( /*mMaxAck*/ ; mMaxAck <= seq; mMaxAck++) {
            mTape.sync(mMaxAck);
            _DBG_(std::cout << "DBG : ACK : " << mMaxAck << std::endl;);
        }

        _DBG_(std::cout << "DBG : Processed ACK complete : " << mMaxAck << ".  Seq " << seq << std::endl;);
    } 
}

    //
    // Inbound buffer from a master source
    //
template< typename TTape, typename... TSyncs >
uint64_t TapeSync<TTape, TSyncs...>:: processBuffer( uint64_t seq, const void* data, size_t msgSize )
{

    if (!isPrimary) {
        // Write this message to the tape.  
        auto& buffer = mTape.getBuffer( seq );
        if (buffer.writeRaw( msgSize, data) == nullptr) {
            std::cerr << "Sync failed : cannot recover..." << std::endl;
            throw std::runtime_error("Sync failed");
        }

        mTape.position(seq);

         // If I haven't skipped anything, mark it as ACK'd.
        if (mMaxAck == seq) {
            mTape.sync(seq); 
            mMaxAck = seq + 1;

             // See if we were behind and have caught up.  
            while (mTape.poll( mMaxAck + 1 ) != 0 ) { // poll or raw_poll -- think poll
                ++mMaxAck;
            }
        }
    }
    
    return mMaxAck;
}

#pragma pop
