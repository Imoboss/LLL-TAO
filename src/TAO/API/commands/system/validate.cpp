/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <TAO/API/types/commands/system.h>
#include <Util/include/debug.h>

#include <Legacy/types/script.h>
#include <Legacy/wallet/wallet.h>

#include <LLD/include/global.h>

#include <TAO/API/include/get.h>
#include <TAO/API/include/global.h>
#include <TAO/API/include/json.h>

#include <TAO/Register/types/address.h>
#include <TAO/Register/types/object.h>

/* Global TAO namespace. */
namespace TAO
{

    /* API Layer namespace. */
    namespace API
    {
        /* Validates a register / legacy address */
        encoding::json System::Validate(const encoding::json& params, const bool fHelp)
        {
            encoding::json jsonRet;

            /* Check for address parameter. */
            if(params.find("address") == params.end() )
                throw Exception(-105, "Missing address");

            /* Extract the address, which will either be a legacy address or a sig chain account address */
            std::string strAddress = params["address"].get<std::string>();

            /* The decoded register address */
            TAO::Register::Address hashAddress;

            /* Decode the address string */
            hashAddress.SetBase58(strAddress);

            /* Populate address into response */
            jsonRet["address"] = strAddress;

            /* handle recipient being a register address */
            if(hashAddress.IsValid())
            {
                /* Check to see if this is a legacy address */
                if(hashAddress.IsLegacy())
                {
                    /* Set the valid flag in the response */
                    jsonRet["is_valid"] = true;

                    /* Set type in response */
                    jsonRet["type"] = "LEGACY";

                    #ifndef NO_WALLET
                    /* Legacy address */
                    Legacy::NexusAddress address(strAddress);

                    /* Check that we have the key in this wallet. */
                    if(Legacy::Wallet::Instance().HaveKey(address) || Legacy::Wallet::Instance().HaveScript(address.GetHash256()))
                        jsonRet["is_mine"] = true;
                    else
                        jsonRet["is_mine"] = false;
                    #endif
                }
                else
                {
                    /* Get the state. We only consider an address valid if the state exists in the register database*/
                    TAO::Register::Object state;
                    if(LLD::Register->ReadState(hashAddress, state, TAO::Ledger::FLAGS::LOOKUP))
                    {
                        /* Set the valid flag in the response */
                        jsonRet["is_valid"] = true;

                        /* Set the register type */
                        jsonRet["type"]    = GetRegisterName(state.nType);

                        /* Check if address is owned by current user */
                        uint256_t hashGenesis = Commands::Get<Users>()->GetCallersGenesis(params);
                        if(hashGenesis != 0)
                        {
                            if(state.hashOwner == hashGenesis)
                                jsonRet["is_mine"] = true;
                            else
                                jsonRet["is_mine"] = false;
                        }

                        /* If it is an object register then parse it to add the object_type */
                        if(state.nType == TAO::Register::REGISTER::OBJECT)
                        {
                            /* parse object so that the data fields can be accessed */
                            if(state.Parse())
                                jsonRet["object_type"] = GetStandardName(state.Standard());
                            else
                                jsonRet["is_valid"] = false;
                        }
                    }
                    else
                    {
                        /* Set the valid flag in the response */
                        jsonRet["is_valid"] = false;
                    }
                }
            }
            else
            {
                /* Set the valid flag in the response */
                jsonRet["is_valid"] = false;
            }

            return jsonRet;
        }

    }
}