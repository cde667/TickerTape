#pragma once

#ifndef _DBG_
#  ifdef DEBUG
#    define _DBG_( x ) x
#  else
#    define _DBG_( x ) 
#  endif
#endif
