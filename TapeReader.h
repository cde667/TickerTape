#pragma once

#include "TapeHeaderMessage.h"
#include "detail/TapeReader.h"

template< typename TTape, typename TProcess, typename... TMsgTypes >
class TapeReader
    : public detail::TapeReader< 1, TTape, TProcess, TMsgTypes...>
{

    using TReader = detail::TapeReader< 1, TTape, TProcess, TMsgTypes...>;

  public:
    TapeReader( TTape& tape, TProcess& msgProc )
        : TReader( tape, msgProc )
    {}

    ~TapeReader()
    {}

    using TReader::poll;
};
    


