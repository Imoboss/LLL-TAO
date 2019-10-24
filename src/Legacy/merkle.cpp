/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <Legacy/types/merkle.h>
#include <LLD/include/global.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/chainstate.h>
#include <TAO/Ledger/types/state.h>
#include <TAO/Ledger/types/transaction.h>

#include <Util/include/args.h>
#include <Util/include/debug.h>


namespace Legacy
{

    /* Default Constructor. */
    MerkleTx::MerkleTx()
    : Transaction   ( )
    , hashBlock     (0)
    , vMerkleBranch ( )
    , nIndex        (-1)
    , pBlock        (nullptr)
    {
    }

    /* Copy Constructor. */
    MerkleTx::MerkleTx(const MerkleTx& tx)
    : Transaction   (tx)
    , hashBlock     (tx.hashBlock)
    , vMerkleBranch (tx.vMerkleBranch)
    , nIndex        (tx.nIndex)
    , pBlock        (nullptr)
    {
    }


    /* Move Constructor. */
    MerkleTx::MerkleTx(MerkleTx&& tx) noexcept
    : Transaction   (std::move(tx))
    , hashBlock     (std::move(tx.hashBlock))
    , vMerkleBranch (std::move(tx.vMerkleBranch))
    , nIndex        (std::move(tx.nIndex))
    , pBlock        (nullptr)
    {
    }


    /* Copy assignment. */
    MerkleTx& MerkleTx::operator=(const MerkleTx& tx)
    {
        nVersion      = tx.nVersion;
        nTime         = tx.nTime;
        vin           = tx.vin;
        vout          = tx.vout;
        nLockTime     = tx.nLockTime;
        hashBlock     = tx.hashBlock;
        vMerkleBranch = tx.vMerkleBranch;
        nIndex        = tx.nIndex;
        pBlock        = nullptr;

        return *this;
    }


    /* Move assignment. */
    MerkleTx& MerkleTx::operator=(MerkleTx&& tx) noexcept
    {
        nVersion      = std::move(tx.nVersion);
        nTime         = std::move(tx.nTime);
        vin           = std::move(tx.vin);
        vout          = std::move(tx.vout);
        nLockTime     = std::move(tx.nLockTime);
        hashBlock     = std::move(tx.hashBlock);
        vMerkleBranch = std::move(tx.vMerkleBranch);
        nIndex        = std::move(tx.nIndex);
        pBlock        = nullptr;

        return *this;
    }


    /* Destructor. */
    MerkleTx::~MerkleTx()
    {
        if(pBlock != nullptr)
            delete pBlock;
    }


    /* Constructor */
    MerkleTx::MerkleTx(const Transaction& txIn)
    : Transaction   (txIn)
    , hashBlock     (0)
    , vMerkleBranch ( )
    , nIndex        (-1)
    , pBlock        (nullptr)
    {
    }


    uint32_t MerkleTx::GetDepthInMainChain() const
    {
        if(hashBlock == 0)
            return 0;

        /* Find the block it claims to be in */
        TAO::Ledger::BlockState state;
        if(pBlock != nullptr)
            state = *pBlock;

        else
        {
            if(!LLD::Ledger->ReadBlock(hashBlock, state))
                return 0;

            /* Save the retrieved state so it doesn't need to be re-read every time it is needed */
            pBlock = new TAO::Ledger::BlockState(state);
        }

        if(!state.IsInMainChain())
            return 0;

        return TAO::Ledger::ChainState::nBestHeight.load() - state.nHeight + 1;
    }


    /* Retrieve the number of blocks remaining until transaction outputs are spendable. */
    uint32_t MerkleTx::GetBlocksToMaturity() const
    {
        if(!(IsCoinBase() || IsCoinStake()))
            return 0;

        uint32_t nMaturity;
        uint32_t nDepth = GetDepthInMainChain();

        if(pBlock == nullptr)
        {
            /* GetDepthInMainChain() reads database and assigns pointer if tx is in a block.
             * If it isn't in a block, yet, use the maturity for the current network version
             */
            if(IsCoinBase())
                nMaturity = TAO::Ledger::MaturityCoinBase();
            else
                nMaturity = TAO::Ledger::MaturityCoinStake();
        }

        else
        {
            if(IsCoinBase())
                nMaturity = TAO::Ledger::MaturityCoinBase(*pBlock);
            else
                nMaturity = TAO::Ledger::MaturityCoinStake(*pBlock);
        }

        /* Legacy mainnet maturity blocks need +20 added so that wallet considers immature for 120 blocks.
         * The NEXUS_MATURITY_LEGACY setting value of 100 was kept for backwards compatability within other parts of code.
         */
        if(nMaturity == TAO::Ledger::NEXUS_MATURITY_LEGACY && !config::fTestNet.load())
            nMaturity += 20;

        if(nDepth >= nMaturity)
            return 0;
        else
            return nMaturity - nDepth;
    }
}
