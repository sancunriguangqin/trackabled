//------------------------------------------------------------------------------
/*
    This file is part of trackabled: https://github.com/trackable/trackabled
    Copyright (c) 2012, 2013 Trackable Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <BeastConfig.h>
#include <trackable/unity/rocksdb.h>
#include <test/nodestore/TestBase.h>
#include <trackable/nodestore/DummyScheduler.h>
#include <trackable/nodestore/Manager.h>
#include <trackable/beast/utility/temp_dir.h>
#include <algorithm>

namespace trackable {
namespace NodeStore {

// Tests the Backend interface
//
class Backend_test : public TestBase
{
public:
    void testBackend (
        std::string const& type,
        std::uint64_t const seedValue,
        int numObjectsToTest = 2000)
    {
        DummyScheduler scheduler;

        testcase ("Backend type=" + type);

        Section params;
        beast::temp_dir tempDir;
        params.set ("type", type);
        params.set ("path", tempDir.path());

        beast::xor_shift_engine rng (seedValue);

        // Create a batch
        auto batch = createPredictableBatch (
            numObjectsToTest, rng());

        beast::Journal j;

        {
            // Open the backend
            std::unique_ptr <Backend> backend =
                Manager::instance().make_Backend (params, scheduler, j);

            // Write the batch
            storeBatch (*backend, batch);

            {
                // Read it back in
                Batch copy;
                fetchCopyOfBatch (*backend, &copy, batch);
                BEAST_EXPECT(areBatchesEqual (batch, copy));
            }

            {
                // Reorder and read the copy again
                std::shuffle (
                    batch.begin(),
                    batch.end(),
                    rng);
                Batch copy;
                fetchCopyOfBatch (*backend, &copy, batch);
                BEAST_EXPECT(areBatchesEqual (batch, copy));
            }
        }

        {
            // Re-open the backend
            std::unique_ptr <Backend> backend = Manager::instance().make_Backend (
                params, scheduler, j);

            // Read it back in
            Batch copy;
            fetchCopyOfBatch (*backend, &copy, batch);
            // Canonicalize the source and destination batches
            std::sort (batch.begin (), batch.end (), LessThan{});
            std::sort (copy.begin (), copy.end (), LessThan{});
            BEAST_EXPECT(areBatchesEqual (batch, copy));
        }
    }

    //--------------------------------------------------------------------------

    void run ()
    {
        std::uint64_t const seedValue = 50;

        testBackend ("nudb", seedValue);

    #if TRACKABLE_ROCKSDB_AVAILABLE
        testBackend ("rocksdb", seedValue);
    #endif

    #ifdef TRACKABLE_ENABLE_SQLITE_BACKEND_TESTS
        testBackend ("sqlite", seedValue);
    #endif
    }
};

BEAST_DEFINE_TESTSUITE(Backend,trackable_core,trackable);

}
}
