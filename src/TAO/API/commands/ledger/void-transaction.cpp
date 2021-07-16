/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLC/include/random.h>
#include <LLC/hash/SK.h>

#include <LLD/include/global.h>

#include <TAO/API/include/build.h>
#include <TAO/API/include/conditions.h>
#include <TAO/API/types/commands.h>

#include <TAO/API/users/types/users.h>
#include <TAO/API/types/commands/ledger.h>

#include <TAO/Operation/include/enum.h>
#include <TAO/Operation/include/execute.h>

#include <TAO/Register/include/enum.h>
#include <TAO/Register/include/names.h>
#include <TAO/Register/include/constants.h>

#include <TAO/Ledger/include/create.h>
#include <TAO/Ledger/types/mempool.h>
#include <TAO/Ledger/types/sigchain.h>

#include <Util/templates/datastream.h>

#include <Legacy/include/evaluate.h>

/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {

        /* Reverses a debit or transfer transaction that the caller has made */
        encoding::json Ledger::VoidTransaction(const encoding::json& params, const bool fHelp)
        {
            encoding::json ret;

            /* Authenticate the users credentials */
            if(!Commands::Get<Users>()->Authenticate(params))
                throw Exception(-139, "Invalid credentials");

            /* Get the PIN to be used for this API call */
            SecureString strPIN = Commands::Get<Users>()->GetPin(params, TAO::Ledger::PinUnlock::TRANSACTIONS);

            /* Get the session to be used for this API call */
            Session& session = Commands::Get<Users>()->GetSession(params);

            /* Check for txid parameter. */
            if(params.find("txid") == params.end())
                throw Exception(-50, "Missing txid.");

            /* Lock the signature chain. */
            LOCK(session.CREATE_MUTEX);

            /* Create the transaction. */
            TAO::Ledger::Transaction tx;
            if(!Users::CreateTransaction(session.GetAccount(), strPIN, tx))
                throw Exception(-17, "Failed to create transaction");

            /* Get the transaction id. */
            uint512_t hashTx;
            hashTx.SetHex(params["txid"].get<std::string>());

            /* The transaction to be voided */
            TAO::Ledger::Transaction txVoid;

            /* Read the debit transaction. */
            if(LLD::Ledger->ReadTx(hashTx, txVoid))
            {
                /* Check that the transaction belongs to the caller */
                if(txVoid.hashGenesis != session.GetAccount()->Genesis())
                    throw Exception(-172, "Cannot void a transaction that does not belong to you.");

                /* Loop through all transactions. */
                for(uint32_t nContract = 0; nContract < txVoid.Size(); ++nContract)
                {
                    /* Process the contract and attempt to void it */
                    TAO::Operation::Contract voidContract;

                    if(AddVoid(txVoid[nContract], nContract, voidContract))
                        tx[tx.Size()] = voidContract;
                }
            }
            else
            {
                throw Exception(-40, "Previous transaction not found.");
            }


            /* Check that output was found. */
            if(tx.Size() == 0)
                throw Exception(-174, "Transaction contains no contracts that can be voided");

            /* Add the fee */
            AddFee(tx);

            /* Execute the operations layer. */
            if(!tx.Build())
                throw Exception(-44, "Transaction failed to build");

            /* Sign the transaction. */
            if(!tx.Sign(session.GetAccount()->Generate(tx.nSequence, strPIN)))
                throw Exception(-31, "Ledger failed to sign transaction.");

            /* Execute the operations layer. */
            if(!TAO::Ledger::mempool.Accept(tx))
                throw Exception(-32, "Failed to accept.");

            /* Build a JSON response object. */
            ret["txid"]  = tx.GetHash().ToString();

            return ret;
        }
    }
}