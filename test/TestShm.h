#pragma once

#include <iostream>
#include <cstring>
#include <stdexcept>
#include <atomic>
#include <sys/shm.h>

namespace test_shm
{

    struct Shmem
    {
        key_t     shmKey = 0;
        int       shmId = -1;
        size_t    shmBytes = 0;
        void*     shmLoc = nullptr;
        bool      nuke = false;
        std::atomic<int> *connCount = new std::atomic<int>(0);

      public:
        // Attach to existing
        explicit Shmem( key_t key, bool rdonly) 
        {
            int retryCount = 0;
            shmKey = key;

        retry:
            shmId = shmget( key, 0, 0);

            if ( -1 == shmId ) {
                if (errno == ENOENT) {
                    ++retryCount;

                    if ((retryCount % 100) == 1) {
                        std::cerr << "Error : Could not attach to shared memory segment : START WRITER FIRST!"
                                  << std::endl;
                    }
                    usleep( 100 * 1000 );
                    goto retry;
                }

                std::cerr << "Error : Could not attach to shared memory segment..." << std::endl;
                std::cerr << "Error : " << strerror(errno) << std::endl;
                throw std::runtime_error("Could not connect to shared memroy.");
            }

            // Get the bytes in the segment...
            shmid_ds statbuf;
            shmctl( shmId, IPC_STAT, &statbuf);
            shmBytes = statbuf.shm_segsz;

            // Map it into our process space.  I'm a only reader --> make it read only...
            shmLoc = shmat( shmId, 0, (rdonly?SHM_RDONLY:0) );
            if ( shmLoc == (void*)(-1)) {
                std::cerr << "Error : Could not map shared memory segment into process space." << std::endl;
                std::cerr << "Error : " << strerror(errno) << std::endl;
                throw std::runtime_error("Could not map shared memory segment into process space.");
            }
        }

        // Create a shared memory segment...
        explicit Shmem( key_t key, size_t size, int perm )
        {
            int shmflag = perm | IPC_CREAT;
            shmKey = key;

            // Get the segment...
            shmId = shmget( key, size, shmflag );

            if ( -1 == shmId ) {
                std::cerr << "Error : Could not create shared memory segment..." << std::endl;
                std::cerr << "      : Key " << key << ", Size " << size << ", Flags " << shmflag << std::endl;
                std::cerr << "Error : " << strerror(errno) << std::endl;
                throw std::runtime_error("Could not connect to shared memroy.");
            }

            // Map it into our process space
            shmLoc = shmat( shmId, 0, 0 );
            if ( shmLoc == (void*)(-1)) {
                std::cerr << "Error : Could not map shared memory segment into process space." << std::endl;
                std::cerr << "Error : " << strerror(errno) << std::endl;
                throw std::runtime_error("Could not map shared memory segment into process space.");
            }

            // Store the relevant data...
            shmBytes = size;

            nuke = true;

        }

        // Copy the shared memory segment for use elsewhere...
        Shmem( const Shmem& rhs )
        {
            shmKey = rhs.shmKey;
            shmId = rhs.shmId;
            shmBytes = rhs.shmBytes;
            shmLoc = rhs.shmLoc;
            nuke = rhs.nuke;
            std::atomic<int> *connCount = rhs.connCount;
            ++ (*connCount);
        }

        ~Shmem( )
        {
            if (connCount->load() == 0) {
                if (shmLoc != nullptr) {
                    shmdt( shmLoc );
                }

                if (nuke && shmId != -1) {
                    shmctl( shmId, IPC_RMID, 0);
                }
            }
        }

        //
        // Some simple accessors...
        //
        size_t bytes(void) const 
        {
            return shmBytes;
        }

        void* connect(void) const
        {
            ++ (*connCount);
            return shmLoc;
        }

        void release( void ) const 
        {
           -- (*connCount);
        }

    };
}

