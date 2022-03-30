/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2022

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/


#include <LLD/include/global.h>


#include <TAO/API/include/check.h>
#include <TAO/API/include/extract.h>
#include <TAO/API/include/global.h>
#include <TAO/API/include/get.h>
#include <TAO/API/include/json.h>

#include <TAO/Ledger/include/constants.h>

#include <TAO/API/types/commands.h>

#include <TAO/API/types/commands/system.h>

#include <TAO/Register/include/verify.h>
#include <TAO/API/types/commands/assets.h>

//TODO: WORK IN PROGRESS

// in = token asset 
// if token is part of asset
// asset owner == token address
// return true/false

/* Global TAO namespace. */
namespace TAO::API
{
    /* Verify if token is the token used to tokenize the provided asset */
    encoding::json System::VerifyTokenizedAsset(const encoding::json& jParams, const bool fHelp)
    {
        /*Build JSON Obj */
        encoding::json jRet;

        uint256_t strToken = ExtractToken(jParams);
        uint256_t strAsset = ExtractAddress(jParams,"asset");

        /* Check for pin size. */
        if(strToken.size() == 0)
            throw Exception(-58, "Empty Parameter [", "token", "]"); 
            
        if(strAsset.size() == 0)
            throw Exception(-58, "Empty Parameter [", "asset", "]");

        const TAO::Register::Address hashToken =
            TAO::Register::Address(strToken);
        if(!hashToken.IsValid())
            throw Exception(-57,"Token address not valid");

        TAO::Register::Object tObject ;
        if (!LLD::Register->ReadObject(strAsset, tObject))
            return jRet;

        if(!tObject.IsValid())
            throw Exception(-57,"Asset not valid");

        std::string assetOwnerAddress = TAO::Register::Address(tObject.hashOwner).ToString();
        std::string tokenAddress = hashToken.ToString();

        jRet["valid"] = tokenAddress == assetOwnerAddress ? true : false ;
        jRet["owner-token"] = assetOwnerAddress;

        /*return JSON */
        return jRet;
    }
}
