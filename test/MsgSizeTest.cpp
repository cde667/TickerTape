#include <iostream>
#include "../detail/MessageSize.h"

template< size_t N >
struct sizeTest {
    char buffer[N];

    static size_t size() {
        return N;
    }
};

bool success = true;

void ERROR(int line) {
    std::cerr << "Error : line " << line << "\n";
    success = false;
}

int main( int argc, char ** argv )
{
    using S1 = sizeTest<1>;
    using S2 = sizeTest<2>;
    using S3 = sizeTest<8>;
    using S4 = sizeTest<123>;
    using S5 = sizeTest<727>;
    using S6 = sizeTest<2313>;
    using S7 = sizeTest<4096>;
    using S8 = sizeTest<16384>;

    size_t MessageSize1 = detail::MessageSize< S1, S2, S3, S4>::msgSize;
    if (MessageSize1 != sizeof(S4)) {
        ERROR( __LINE__ );
    }

    MessageSize1 = detail::MessageSize<>::msgSize;
    if (MessageSize1 != 0) {
        ERROR( __LINE__ );
    }

    MessageSize1 = detail::MessageSize<S1, S8, S2, S4>::msgSize;
    if ( MessageSize1 != sizeof(S8) ) {
        ERROR( __LINE__ );
    }


    MessageSize1 = detail::MessageSize< S1, S7, S7>::msgSize;
    if (MessageSize1 !=  sizeof(S7) ) {
        ERROR( __LINE__ );
    }


    MessageSize1 = detail::MessageSize< S2, S4, S7, S6>::msgSize;
    if (MessageSize1 != sizeof(S7)) {
        ERROR( __LINE__ );
    }


    MessageSize1 = detail::MessageSize<S1, S2, S3, S4, S5, S6, S7, S8>::msgSize;
    if (MessageSize1 != sizeof(S8)) {
        ERROR( __LINE__ );
    }

    if (success) {
        std::cout << "Test passed.\n";
    }
    else {
        std::cerr << "TEST FAILED!\n";
    }

    return (success?0:1);
};
