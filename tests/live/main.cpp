/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLC/types/uint1024.h>
#include <LLC/hash/SK.h>
#include <LLC/include/random.h>

#include <LLD/include/global.h>
#include <LLD/cache/binary_lru.h>
#include <LLD/keychain/hashmap.h>
#include <LLD/templates/sector.h>

#include <Util/include/debug.h>
#include <Util/include/base64.h>

#include <openssl/rand.h>

#include <LLC/hash/argon2.h>
#include <LLC/hash/SK.h>
#include <LLC/include/flkey.h>
#include <LLC/types/bignum.h>

#include <Util/include/hex.h>

#include <iostream>

#include <TAO/Register/types/address.h>
#include <TAO/Register/types/object.h>

#include <TAO/Register/include/create.h>

#include <TAO/Ledger/types/genesis.h>
#include <TAO/Ledger/types/sigchain.h>
#include <TAO/Ledger/types/transaction.h>

#include <TAO/Ledger/include/ambassador.h>

#include <Legacy/types/address.h>
#include <Legacy/types/transaction.h>

#include <LLP/templates/ddos.h>
#include <Util/include/runtime.h>

#include <list>
#include <functional>
#include <variant>

#include <Util/include/softfloat.h>
#include <Util/include/filesystem.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/stake.h>

#include <LLP/types/tritium.h>

#include <TAO/Ledger/include/chainstate.h>
#include <TAO/Ledger/types/locator.h>

#include <LLD/config/hashmap.h>
#include <LLD/config/sector.h>

class TestDB : public LLD::Templates::StaticDatabase<LLD::BinaryHashMap, LLD::BinaryLRU, LLD::Config::Hashmap>
{
public:
    TestDB(const LLD::Config::Sector& sector, const LLD::Config::Hashmap& keychain)
    : StaticDatabase(sector, keychain)
    {
    }

    ~TestDB()
    {

    }

    bool WriteKey(const uint1024_t& key, const uint1024_t& value)
    {
        return Write(std::make_pair(std::string("key"), key), value);
    }


    bool ReadKey(const uint1024_t& key, uint1024_t &value)
    {
        return Read(std::make_pair(std::string("key"), key), value);
    }


    bool EraseKey(const uint1024_t& key)
    {
        return Erase(std::make_pair(std::string("key"), key));
    }


    bool WriteLast(const uint1024_t& last)
    {
        return Write(std::string("last"), last);
    }

    bool ReadLast(uint1024_t &last)
    {
        return Read(std::string("last"), last);
    }

};

#include <TAO/Ledger/include/genesis_block.h>

const uint256_t hashSeed = 55;

#include <bitset>

#include <LLP/include/global.h>

#include <LLD/cache/template_lru.h>


/* Read an index entry at given bucket crossing file boundaries. */
bool read_index(const uint32_t nBucket, const uint32_t nTotal, const LLD::Config::Hashmap& CONFIG)
{
    const uint32_t INDEX_FILTER_SIZE = 10;

    /* Check we are flushing within range of the hashmap indexes. */
    if(nBucket >= CONFIG.HASHMAP_TOTAL_BUCKETS)
        return debug::error(FUNCTION, "out of range ", VARIABLE(nBucket));

    /* Keep track of how many buckets and bytes we have remaining in this read cycle. */
    uint32_t nRemaining = nTotal;
    uint32_t nIterator  = nBucket;

    /* Calculate the number of bukcets per index file. */
    const uint32_t nTotalBuckets = (CONFIG.HASHMAP_TOTAL_BUCKETS / CONFIG.MAX_FILES_PER_INDEX) + 1;
    const uint32_t nMaxSize = nTotalBuckets * INDEX_FILTER_SIZE;

    /* Adjust our buffer size to fit the total buckets. */
    do
    {
        /* Calculate the file and boundaries we are on with current bucket. */
        const uint32_t nFile = (nIterator * CONFIG.MAX_FILES_PER_INDEX) / CONFIG.HASHMAP_TOTAL_BUCKETS;
        const uint32_t nEnd  = (((nFile + 1) * CONFIG.HASHMAP_TOTAL_BUCKETS) / CONFIG.MAX_FILES_PER_INDEX);

        //const uint32_t nEnd2  = (((nFile + 2) * CONFIG.HASHMAP_TOTAL_BUCKETS) / CONFIG.MAX_FILES_PER_INDEX);
        //const uint64_t nCommon = (nEnd * CONFIG.MAX_FILES_PER_INDEX) / CONFIG.HASHMAP_TOTAL_BUCKETS;

        const uint32_t nBegin = (nFile * CONFIG.HASHMAP_TOTAL_BUCKETS) / CONFIG.MAX_FILES_PER_INDEX;
        const uint32_t nCheck = (nBegin * CONFIG.MAX_FILES_PER_INDEX) / CONFIG.HASHMAP_TOTAL_BUCKETS;

        const bool fAdjust = (nCheck != nFile);

        /* Find our new file position from current bucket and offset. */
        const uint64_t nFilePos      = (INDEX_FILTER_SIZE * (nIterator - (fAdjust ? 1 : 0) -
            ((nFile * CONFIG.HASHMAP_TOTAL_BUCKETS) / CONFIG.MAX_FILES_PER_INDEX)));

        /* Find our new file position from current bucket and offset. */
        //const uint64_t nFilePos2      = (INDEX_FILTER_SIZE * (//(nFile == 0 ? 0 : 1) -
        //    ((nEnd * CONFIG.HASHMAP_TOTAL_BUCKETS) / CONFIG.MAX_FILES_PER_INDEX)));

        /* Find the range (in bytes) we want to read for this index range. */
        const uint32_t nMaxBuckets = std::min(nRemaining, std::max(1u, (nEnd - nIterator)));


        if(nFilePos + INDEX_FILTER_SIZE > nMaxSize)
        {
            debug::warning(fAdjust ? ANSI_COLOR_BRIGHT_YELLOW : "",
                          VARIABLE(nRemaining),  " | ",
                          VARIABLE(nMaxBuckets), " | ",
                          VARIABLE(nFilePos),    " | ",
                          VARIABLE(nFile),       " | ",
                          VARIABLE(nEnd),        " | ",
                          VARIABLE(nBegin),      " | ",
                          VARIABLE(nCheck),      " | ",
                          VARIABLE(nBucket),     " | ",
                          fAdjust ? ANSI_COLOR_RESET : ""
                      );

            return false;
        }

        /* Track our current binary position, remaining buckets to read, and current bucket iterator. */
        nRemaining -= nMaxBuckets;
        nIterator  += nMaxBuckets;
    }
    while(nRemaining > 0); //continue reading into the buffer, with one loop per file adjusting to each boundary

    return true;
}

#include <leveldb/c.h>


/* This is for prototyping new code. This main is accessed by building with LIVE_TESTS=1. */
int main(int argc, char** argv)
{
    rlimit rlim;

    int nRet = getrlimit(RLIMIT_AS, &rlim);
    debug::log(0, VARIABLE(nRet), " | ", VARIABLE(rlim.rlim_cur), " | ", VARIABLE(rlim.rlim_max));


    nRet = getrlimit(RLIMIT_MEMLOCK, &rlim);
    debug::log(0, VARIABLE(nRet), " | ", VARIABLE(rlim.rlim_cur), " | ", VARIABLE(rlim.rlim_max));

    config::ParseParameters(argc, argv);

    LLP::Initialize();

    //config::nVerbose.store(4);
    config::mapArgs["-datadir"] = "/database";

    const std::string strDB = "LLD";

    std::string strIndex = config::mapArgs["-datadir"] + "/" + strDB + "/";

    if(config::GetBoolArg("-reset", false))
    {
        debug::log(0, ANSI_COLOR_BRIGHT_YELLOW, "Deleting data directory ", strIndex, ANSI_COLOR_RESET);
        filesystem::remove_directories(strIndex);
    }

    std::vector<uint1024_t> vFirst;


    bool fNew = false;

    std::string strKeys = config::mapArgs["-datadir"] + "/keys";
    if(!filesystem::exists(strKeys))
    {
        /* Create 10k new keys. */
        std::vector<uint1024_t> vKeys;
        for(int i = 0; i < 10000; ++i)
            vKeys.push_back(LLC::GetRand1024());

        /* Create a new file for next writes. */
        std::fstream stream(strKeys, std::ios::out | std::ios::binary | std::ios::trunc);
        stream.write((char*)&vKeys[0], vKeys.size() * sizeof(vKeys[0]));
        stream.close();

        debug::log(0, "Created new keys file.");
        vFirst = vKeys;

        fNew = true;
    }
    else
    {
        /* Create 10k new keys. */
        std::vector<uint1024_t> vKeys(10000, 0);

        /* Create a new file for next writes. */
        //std::fstream stream(strKeys, std::ios::out | std::ios::binary | std::ios::in);
        //stream.read((char*)&vKeys[0], vKeys.size() * sizeof(vKeys[0]));
        //stream.close();

        runtime::stopwatch swMap;
        if(config::GetBoolArg("-fstream"))
        {
            swMap.start();
            std::fstream stream(strKeys, std::ios::out | std::ios::binary | std::ios::in);
            stream.read((char*)&vKeys[0], vKeys.size() * sizeof(vKeys[0]));
            stream.close();
            swMap.stop();

            debug::log(0, "fstream completed in ", swMap.ElapsedMicroseconds());
        }
        else
        {
            swMap.start();

            //file_t hFile = open_file(strKeys, )
            memory::mstream stream2(strKeys, std::ios::in | std::ios::out);
            if(!stream2.read((char*)&vKeys[0], vKeys.size() * sizeof(vKeys[0])))
                return debug::error("Failed to read data for mmap");

            swMap.stop();

            stream2.seekp((vKeys.size() - 1) * sizeof(vKeys[0]));
            if(!stream2.write((char*)&vKeys[2], sizeof(vKeys[2])))
                return debug::error("Failed to write a new mapping");

            stream2.flush();

            stream2.seekg(0, std::ios::beg);

            std::vector<uint8_t> vData(vKeys.size() * sizeof(vKeys[0]), 0);
            if(!stream2.read((char*)&vData[0], vData.size()))
                return debug::error("Failed to read data for mmap");

            stream2.close();

            debug::log(0, "MMap completed in ", swMap.ElapsedMicroseconds());
        }

        vFirst = vKeys;

        debug::log(0, "[A] ", vFirst.begin()->SubString());
        debug::log(0, "[B] ", vFirst.back().SubString());
    }


    #if 0
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
    char *err = NULL;

    /******************************************/
    /* OPEN */

    options = leveldb_options_create();
    leveldb_options_set_create_if_missing(options, 1);

    db = leveldb_open(options, "/database/leveldb", &err);

    if (err != NULL) {
        return debug::error("Failed to open LEVELDB instance");
    }

    /* reset error var */
    leveldb_free(err); err = NULL;
    #endif



    //build our base configuration
    LLD::Config::Base BASE =
        LLD::Config::Base(strDB, LLD::FLAGS::CREATE | LLD::FLAGS::WRITE);

    //build our sector configuration
    LLD::Config::Sector SECTOR      = LLD::Config::Sector(BASE);
    SECTOR.MAX_SECTOR_FILE_STREAMS  = 16;
    SECTOR.MAX_SECTOR_BUFFER_SIZE   = 1024 * 1024 * 4; //4 MB write buffer
    SECTOR.MAX_SECTOR_CACHE_SIZE    = 256; //4 MB of cache available

    //build our hashmap configuration
    LLD::Config::Hashmap CONFIG     = LLD::Config::Hashmap(BASE);
    CONFIG.HASHMAP_TOTAL_BUCKETS    = 20000000;
    CONFIG.MAX_FILES_PER_HASHMAP    = 4;
    CONFIG.MAX_FILES_PER_INDEX      = 10;
    CONFIG.MAX_HASHMAPS             = 50;
    CONFIG.MIN_LINEAR_PROBES        = 1;
    CONFIG.MAX_LINEAR_PROBES        = 1024;
    CONFIG.MAX_HASHMAP_FILE_STREAMS = 100;
    CONFIG.MAX_INDEX_FILE_STREAMS   = 10;
    CONFIG.PRIMARY_BLOOM_HASHES     = 7;
    CONFIG.PRIMARY_BLOOM_ACCURACY   = 300;
    CONFIG.SECONDARY_BLOOM_BITS     = 13;
    CONFIG.SECONDARY_BLOOM_HASHES   = 7;
    CONFIG.QUICK_INIT               = !config::GetBoolArg("-audit", false);

    TestDB* bloom = new TestDB(SECTOR, CONFIG);

    runtime::stopwatch swElapsed, swReaders;

    runtime::stopwatch swElapsed1, swReaders1;

    uint32_t nTotalWritten = 0, nTotalRead = 0;;

    uint32_t nTotalWritten1 = 0, nTotalRead1 = 0;;


    {
        debug::log(0, ANSI_COLOR_BRIGHT_GREEN, "[LLD] BASE LEVEL TEST:", ANSI_COLOR_RESET);

        runtime::stopwatch swTimer;
        swTimer.start();
        swElapsed.start();

        uint32_t nTotal = 0;
        if(fNew)
        {
            for(const auto& nBucket : vFirst)
            {
                ++nTotalWritten;

                ++nTotal;
                if(!bloom->WriteKey(nBucket, nBucket))
                    return debug::error(FUNCTION, "failed to write ", VARIABLE(nTotal));
            }


            swElapsed.stop();
            swTimer.stop();

            uint64_t nElapsed = swTimer.ElapsedMicroseconds();
            debug::log(0,  "[LLD] ", vFirst.size() / 1000, "k records written in ", nElapsed,
                ANSI_COLOR_BRIGHT_YELLOW, " (", std::fixed, (1000000.0 * vFirst.size()) / nElapsed, " writes/s)", ANSI_COLOR_RESET);

        }


        uint1024_t hashKey = 0;

        swTimer.reset();
        swTimer.start();

        //swReaders.start();

        nTotal = 0;
        for(const auto& nBucket : vFirst)
        {
            ++nTotal;
            ++nTotalRead;

            if(!bloom->ReadKey(nBucket, hashKey))
                return debug::error("Failed to read ", nBucket.SubString(), " total ", nTotal);
        }
        swTimer.stop();
        //swReaders.stop();
        nTotal = 0;

        uint64_t nElapsed = swTimer.ElapsedMicroseconds();
        debug::log(0, "[LLD] ", vFirst.size() / 1000, "k records read in ", nElapsed,
            ANSI_COLOR_BRIGHT_YELLOW, " (", std::fixed, (1000000.0 * vFirst.size()) / nElapsed, " read/s)", ANSI_COLOR_RESET);
    }



    #if 0
    {
        debug::log(0, ANSI_COLOR_BRIGHT_RED, "[LEVELDB] BASE LEVEL TEST:", ANSI_COLOR_RESET);

        runtime::stopwatch swTimer;
        swTimer.start();
        swElapsed1.start();

        woptions = leveldb_writeoptions_create();

        uint32_t nTotal = 0;
        if(fNew)
        {

            for(const auto& nBucket : vFirst)
            {
                ++nTotalWritten1;
                ++nTotal;

                /******************************************/
                /* WRITE */
                std::string strData = nBucket.ToString();
                leveldb_put(db, woptions, strData.c_str(), strData.size(), strData.c_str(), strData.size(), &err);

                if (err != NULL) {
                  return debug::error("[LEVELDB] Write Failed for ", nBucket.ToString());
                }
            }

            leveldb_free(err); err = NULL;

            swElapsed1.stop();
            swTimer.stop();

            uint64_t nElapsed = swTimer.ElapsedMicroseconds();
            debug::log(0, "[LEVELDB] ", vFirst.size() / 1000, "k records written in ", nElapsed,
                ANSI_COLOR_BRIGHT_RED, " (", std::fixed, (1000000.0 * vFirst.size()) / nElapsed, " writes/s)", ANSI_COLOR_RESET);

        }

        uint1024_t hashKey = 0;

        swTimer.reset();
        swTimer.start();

        swReaders1.start();


        roptions = leveldb_readoptions_create();

        nTotal = 0;
        for(const auto& nBucket : vFirst)
        {
            ++nTotal;
            ++nTotalRead1;
            /******************************************/
            /* READ */

            size_t read_len;
            std::string strKey = nBucket.ToString();
            char* strRead;
            strRead = leveldb_get(db, roptions, strKey.c_str(), strKey.size(), &read_len, &err);

            if (err != NULL) {
             return debug::error("[LEVELDB] Read Failed at ", nBucket.ToString(), " value ", strRead);
            }

            leveldb_free(err); err = NULL;
        }
        swTimer.stop();
        swReaders1.stop();

        uint64_t nElapsed = swTimer.ElapsedMicroseconds();
        debug::log(0, "[LEVELDB] ", vFirst.size() / 1000, "k records read in ", nElapsed,
            ANSI_COLOR_BRIGHT_RED, " (", std::fixed, (1000000.0 * vFirst.size()) / nElapsed, " read/s)", ANSI_COLOR_RESET);

        //if(!bloom->EraseKey(vKeys[0]))
        //    return debug::error("failed to erase ", vKeys[0].SubString());

        //if(bloom->ReadKey(vKeys[0], hashKey))
        //    return debug::error("Failed to erase ", vKeys[0].SubString());
    }
    #endif


    for(int n = 0; n < config::GetArg("-tests", 1); ++n)
    {
        debug::log(0, ANSI_COLOR_BRIGHT_CYAN, "Generating Keys for test: ", n, "/", config::GetArg("-tests", 1), ANSI_COLOR_RESET);

        std::vector<uint1024_t> vKeys;
        for(int i = 0; i < config::GetArg("-total", 1); ++i)
            vKeys.push_back(LLC::GetRand1024());


        runtime::stopwatch swTimer;
        swTimer.start();
        swElapsed.start();

        uint32_t nTotal = 0;
        for(const auto& nBucket : vKeys)
        {
            ++nTotalWritten;

            ++nTotal;
            if(!bloom->WriteKey(nBucket, nBucket))
                return debug::error(FUNCTION, "failed to write ", VARIABLE(nTotal));
        }


        swElapsed.stop();
        swTimer.stop();

        uint64_t nElapsed = swTimer.ElapsedMicroseconds();
        debug::log(0,  "[LLD] ", vKeys.size() / 1000, "k records written in ", nElapsed,
            ANSI_COLOR_BRIGHT_YELLOW, " (", std::fixed, (1000000.0 * vKeys.size()) / nElapsed, " writes/s)", ANSI_COLOR_RESET);

        uint1024_t hashKey = 0;

        swTimer.reset();
        swTimer.start();

        swReaders.start();

        nTotal = 0;
        for(const auto& nBucket : vKeys)
        {
            ++nTotal;
            ++nTotalRead;

            bloom->ReadKey(nBucket, hashKey);
                //return debug::error("Failed to read ", nBucket.SubString(), " total ", nTotal);
        }
        swTimer.stop();
        swReaders.stop();

        nElapsed = swTimer.ElapsedMicroseconds();
        debug::log(0, "[LLD] ", vKeys.size() / 1000, "k records read in ", nElapsed,
            ANSI_COLOR_BRIGHT_YELLOW, " (", std::fixed, (1000000.0 * vKeys.size()) / nElapsed, " read/s)", ANSI_COLOR_RESET);

        //if(!bloom->EraseKey(vKeys[0]))
        //    return debug::error("failed to erase ", vKeys[0].SubString());

        //if(bloom->ReadKey(vKeys[0], hashKey))
        //    return debug::error("Failed to erase ", vKeys[0].SubString());
    }


    {
        /******************************************/
        /* CLOSE */

        runtime::stopwatch swClose;
        swClose.start();
        //swElapsed.start();
        delete bloom;
        swClose.stop();
        //swElapsed.stop();

        debug::log(0, "[LLD] Closed in ", swClose.ElapsedMilliseconds(), " ms");
    }



    #if 0
    for(int n = 0; n < config::GetArg("-tests", 1); ++n)
    {
        debug::log(0, ANSI_COLOR_BRIGHT_CYAN, "Generating Keys for test: ", n, "/", config::GetArg("-tests", 1), ANSI_COLOR_RESET);

        std::vector<uint1024_t> vKeys;
        for(int i = 0; i < config::GetArg("-total", 1); ++i)
            vKeys.push_back(LLC::GetRand1024());

        {
            runtime::stopwatch swTimer;
            swTimer.start();
            swElapsed1.start();

            woptions = leveldb_writeoptions_create();

            uint32_t nTotal = 0;
            for(const auto& nBucket : vKeys)
            {
                ++nTotalWritten1;

                ++nTotal;

                /******************************************/
                /* WRITE */
                std::string strData = nBucket.ToString();
                leveldb_put(db, woptions, strData.c_str(), strData.size(), strData.c_str(), strData.size(), &err);

                if (err != NULL) {
                  return debug::error("[LEVELDB] Write Failed for ", nBucket.ToString());
                }
            }

            leveldb_free(err); err = NULL;


            swElapsed1.stop();
            swTimer.stop();

            uint64_t nElapsed = swTimer.ElapsedMicroseconds();
            debug::log(0, "[LEVELDB] ", vKeys.size() / 1000, "k records written in ", nElapsed,
                ANSI_COLOR_BRIGHT_RED, " (", std::fixed, (1000000.0 * vKeys.size()) / nElapsed, " writes/s)", ANSI_COLOR_RESET);

            uint1024_t hashKey = 0;

            swTimer.reset();
            swTimer.start();

            swReaders1.start();


            roptions = leveldb_readoptions_create();

            nTotal = 0;
            for(const auto& nBucket : vKeys)
            {
                ++nTotal;
                ++nTotalRead1;
                /******************************************/
                /* READ */

                size_t read_len;
                std::string strKey = nBucket.ToString();
                char* strRead;
                strRead = leveldb_get(db, roptions, strKey.c_str(), strKey.size(), &read_len, &err);

                if (err != NULL) {
                 return debug::error("[LEVELDB] Read Failed at ", nBucket.ToString(), " value ", strRead);
                }

                leveldb_free(err); err = NULL;
            }
            swTimer.stop();
            swReaders1.stop();

            nElapsed = swTimer.ElapsedMicroseconds();
            debug::log(0, "[LEVELDB] ", vKeys.size() / 1000, "k records read in ", nElapsed,
                ANSI_COLOR_BRIGHT_RED, " (", std::fixed, (1000000.0 * vKeys.size()) / nElapsed, " read/s)", ANSI_COLOR_RESET);

            //if(!bloom->EraseKey(vKeys[0]))
            //    return debug::error("failed to erase ", vKeys[0].SubString());

            //if(bloom->ReadKey(vKeys[0], hashKey))
            //    return debug::error("Failed to erase ", vKeys[0].SubString());
        }

    }

    {
        /******************************************/
        /* CLOSE */

        runtime::stopwatch swClose;
        swClose.start();
        swElapsed1.start();
        leveldb_close(db);
        swClose.stop();
        swElapsed1.stop();

        debug::log(0, "[LEVELDB] Closed in ", swClose.ElapsedMilliseconds(), " ms");
    }

    #endif


    {
        uint64_t nElapsed = swElapsed1.ElapsedMicroseconds();
        uint64_t nMinutes = nElapsed / 60000000;
        uint64_t nSeconds = (nElapsed / 1000000) - (nMinutes * 60);

        debug::log(0, ANSI_COLOR_BRIGHT_RED, "[LEVELDB] Completed Writing ", nTotalWritten1, " Keys in ",
            nMinutes, " minutes ", nSeconds, " seconds (", std::fixed, (1000000.0 * nTotalWritten1) / nElapsed, " writes/s)");
    }

    {
        uint64_t nElapsed = swReaders1.ElapsedMicroseconds();
        uint64_t nMinutes = nElapsed / 60000000;
        uint64_t nSeconds = (nElapsed / 1000000) - (nMinutes * 60);

        debug::log(0, ANSI_COLOR_BRIGHT_RED, "[LEVELDB] Completed Reading ", nTotalRead1, " Keys in ",
            nMinutes, " minutes ", nSeconds, " seconds (", std::fixed, (1000000.0 * nTotalRead1) / nElapsed, " reads/s)");
    }


    {
        uint64_t nElapsed = swElapsed.ElapsedMicroseconds();
        uint64_t nMinutes = nElapsed / 60000000;
        uint64_t nSeconds = (nElapsed / 1000000) - (nMinutes * 60);

        debug::log(0, ANSI_COLOR_BRIGHT_YELLOW, "[LLD] Completed Writing ", nTotalWritten, " Keys in ",
            nMinutes, " minutes ", nSeconds, " seconds (", std::fixed, (1000000.0 * nTotalWritten) / nElapsed, " writes/s)");
    }

    {
        uint64_t nElapsed = swReaders.ElapsedMicroseconds();
        uint64_t nMinutes = nElapsed / 60000000;
        uint64_t nSeconds = (nElapsed / 1000000) - (nMinutes * 60);

        debug::log(0, ANSI_COLOR_BRIGHT_YELLOW, "[LLD] Completed Reading ", nTotalRead, " Keys in ",
            nMinutes, " minutes ", nSeconds, " seconds (", std::fixed, (1000000.0 * nTotalRead) / nElapsed, " reads/s)");
    }







    return 0;
}
