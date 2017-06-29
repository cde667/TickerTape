#pragma once

#include <cstdint>
#include <cstring>
#include <sys/time.h>

namespace sockets
{
    namespace SocketMsgs
    {
        static const uint64_t MAX_SEQ = 0xFFFFFFFFFFFFFFFF;

        enum SocketMsgType : uint8_t
        {
            eHeartbeat = 0,
            eKeepAlive = 1,
            eTapeBuffer = 2,
            eAck = 3,
            eResendRequest = 4
        };

        struct Header 
        {
            SocketMsgType msgType;
            uint64_t mSequence;

            Header( SocketMsgType t, uint64_t sequence )
                : msgType(t), mSequence(sequence)
            {}
        };

        struct Heartbeat 
            : public Header
        {
            struct timeval tv;

            Heartbeat()
                : Header( eHeartbeat, MAX_SEQ )
            { 
                gettimeofday( &tv, nullptr) ;
            }
        };

        struct KeepAlive
            : public Header
        {
            struct timeval tv;

            KeepAlive()
                : Header( eKeepAlive, MAX_SEQ )
            {
                gettimeofday( &tv, nullptr );
            }
        };

        struct SendBuffer 
            : public Header
        {
            size_t  mBytes;

            SendBuffer( uint64_t seq, size_t bytes )
                : Header( eTapeBuffer, seq )
                , mBytes(bytes)
            { }
        };

        struct Ack 
            : public Header
        {
            uint64_t mAckSeqBegin;
            uint64_t mAckSeqEnd;

            Ack( uint64_t seq1, uint64_t seq2 )
                : Header( eAck, MAX_SEQ )
                , mAckSeqBegin( seq1 )
                , mAckSeqEnd( seq2 )
            { }

        };

        struct ResendRequest 
            : public Header
        {
            uint64_t mBeginSeq;
            uint64_t mEndSeq;

            ResendRequest( uint64_t begin, uint64_t end )
                : Header( eResendRequest, MAX_SEQ )
                , mBeginSeq(begin), mEndSeq(end)
            { }

        };

    }

} // namespace socket

