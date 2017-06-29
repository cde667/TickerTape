
#pragma push(pack, 1)

#include <iostream>
#include <vector>

//
// TODO : size_t and uint64_t are used for sequence numbers.  Either standardize on uint64_t across all
// things --or-- create a TSequenceNumber and use that.  (preferably the later)
//

namespace detail
{


    template< typename... TSyncs >
    class Sync;


    template< typename TSync, typename... TSyncs >
    class Sync<TSync, TSyncs...>
        : public Sync< TSyncs... >
    {
        std::vector< TSync* > mSyncs;

      public:
        Sync() {}

        using Sync<TSyncs...>::addSync;
        using Sync<TSyncs...>::removeSync;

        void addSync( TSync& sync )
        {
            mSyncs.push_back( &sync );
        }

        void removeSync( TSync& sync )
        {
            for (auto iter = mSyncs.begin(); iter != mSyncs.end(); ++iter) {
                if ( (*iter) == &sync ) {
                    mSyncs.erase( iter );
                    break;
                }
            }
        }

        // this seems to be the right model for sending it to the next level;
        // it doesn't seem to be the right model for getting back the acks...
        void doSync( uint64_t seq, const void* buffer, size_t size );

        // read () --
        //   See what data is available.  We then want to send it back
        //   back thru the Sync.  I don't feel that this part is quite
        //   the right structure; but it should work.
        template< typename TSynchronizer >
        void read( TSynchronizer& sync );
    };

    template<>
    class Sync<>
    {

      public:
        void addSync(){}
        void removeSync(){}

        void doSync( uint64_t seq, const void* buffer, size_t size )
        { }

        template< typename TSync >
        void read( const TSync& sync )
        { }
    };

}


//
// Sync Client
//   - one connection
//   - connects out
// Sync Server
//   - listener
//   - any # of connections
//

template< typename TTape, typename... TSyncs >
class TapeSync
    : public detail::Sync< TSyncs... >
{
    TTape& mTape;

    bool isPrimary = false;
    uint64_t mPosition = 0; // tape position
    uint64_t mMaxAck = 0;   // we have cleared up to THIS sequence number
    
  public:
    TapeSync( TTape& tape, bool primary )
        : mTape(tape), isPrimary(primary)
    { }

    template< typename T >
    void addSync( T& sync )
    {
        detail::Sync<TSyncs...>::addSync(sync);
    }

    template< typename T >
    void removeSync( T& sync )
    {
        detail::Sync<TSyncs...>::removeSync(sync);
    }

    void poll( );

    //
    // Process the ACKs coming back
    //:
    void processClient( uint64_t seq );

    //
    // Inbound buffer from a master source
    //
    uint64_t processBuffer( uint64_t seq, const void* buffer, size_t msgSize );

    template< typename TSync >
    void processResend( TSync& client, uint64_t start, uint64_t end )
    {

        // TODO : break this into two cases.  First, for small resends, just send it.
        // For larger ones, we should move it into another thread.
        for ( /*start*/; start <= end; ++start ) {
            auto& buffer = mTape.readBuffer( start );
            client.sendBuffer( start, &buffer, buffer.size() );
        }
    }

};

#pragma pop
