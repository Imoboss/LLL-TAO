/*__________________________________________________________________________________________

			(c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

			(c) Copyright The Nexus Developers 2014 - 2019

			Distributed under the MIT software license, see the accompanying
			file COPYING or http://www.opensource.org/licenses/mit-license.php.

			"ad vocem populi" - To The Voice of The People

____________________________________________________________________________________________*/

#include <LLC/include/random.h>

#include <LLD/include/global.h>
#include <LLD/hash/xxh3.h>

#include <TAO/API/include/check.h>
#include <TAO/API/include/extract.h>

#include <TAO/API/types/authentication.h>

#include <TAO/Register/types/address.h>
#include <TAO/Register/types/object.h>

/* Global TAO namespace. */
namespace TAO::API
{
    /* Static initialization of mutex. */
    std::recursive_mutex Authentication::MUTEX;


    /* Static initialize of map sessions. */
    std::map<uint256_t, Authentication::Session> Authentication::mapSessions;


    /* Static initialize of map locks. */
    std::vector<std::recursive_mutex> Authentication::vLocks;


    /* Initializes the current authentication systems. */
    void Authentication::Initialize()
    {
        /* Create our session locks. */
        vLocks = std::vector<std::recursive_mutex>(config::GetArg("-sessionlocks", 1024));
    }


    /* Insert a new session into authentication map. */
    void Authentication::Insert(const uint256_t& hashSession, Session& rSession)
    {
        RECURSIVE(MUTEX);

        /* Add the new session to sessions map. */
        mapSessions.insert(std::make_pair(hashSession, std::move(rSession)));
    }


    /* Check if user is already authenticated by genesis-id. */
    bool Authentication::Active(const uint256_t& hashGenesis, uint256_t &hashSession)
    {
        RECURSIVE(MUTEX);

        /* Loop through sessions map. */
        for(const auto& rSession : mapSessions)
        {
            /* Check genesis to session. */
            if(rSession.second.Genesis() == hashGenesis)
            {
                /* Set our returned session hash. */
                hashSession = rSession.first;

                /* Check if active. */
                if(rSession.second.Active() > config::GetArg("-inactivetimeout", 3600))
                    return false;

                return true;
            }
        }

        return false;
    }


    /* Get the genesis-id of the given caller using session from params. */
    bool Authentication::Caller(const encoding::json& jParams, uint256_t &hashCaller)
    {
        RECURSIVE(MUTEX);

        /* Get the current session-id. */
        const uint256_t hashSession =
            ExtractHash(jParams, "session", default_session());

        /* Check for active session. */
        if(!mapSessions.count(hashSession))
            return false;

        /* Get a copy of our current active session. */
        const Session& rSession =
            mapSessions[hashSession];

        /* Set the caller from our session data. */
        hashCaller = rSession.Genesis();

        return true;
    }


    /* Get the genesis-id of the given caller using session from params. */
    uint256_t Authentication::Caller(const encoding::json& jParams)
    {
        RECURSIVE(MUTEX);

        /* Get the current session-id. */
        const uint256_t hashSession =
            ExtractHash(jParams, "session", default_session());

        /* Check for active session. */
        if(!mapSessions.count(hashSession))
            throw Exception(-11, "Session not found");

        /* Get a copy of our current active session. */
        const Session& rSession =
            mapSessions[hashSession];

        /* Set the caller from our session data. */
        return rSession.Genesis();
    }


    /* Get an instance of current session credentials indexed by session-id. */
    const memory::encrypted_ptr<TAO::Ledger::SignatureChain>& Authentication::Credentials(const encoding::json& jParams)
    {
        RECURSIVE(MUTEX);

        /* Get the current session-id. */
        const uint256_t hashSession =
            ExtractHash(jParams, "session", default_session());

        /* Check for active session. */
        if(!mapSessions.count(hashSession))
            throw Exception(-9, "Session doesn't exist");

        /* Get a copy of our current active session. */
        return mapSessions[hashSession].Credentials();
    }


    /* Unlock and get the active pin from current session. */
    std::recursive_mutex& Authentication::Unlock(const encoding::json& jParams, SecureString &strPIN, const uint8_t nRequestedActions)
    {
        /* Get the current session-id. */
        const uint256_t hashSession =
            ExtractHash(jParams, "session", default_session());

        {
            RECURSIVE(MUTEX);

            /* Check for active session. */
            if(!mapSessions.count(hashSession))
                throw Exception(-9, "Session doesn't exist");

            /* Get a copy of our current active session. */
            const Session& rSession =
                mapSessions[hashSession];

            /* Check that this is a local session. */
            if(rSession.Type() != Session::LOCAL)
                throw Exception(-9, "Only local sessions can be unlocked");

            /* Check for password requirement field. */
            if(config::GetBoolArg("-requirepassword", false))
            {
                /* Grab our password from parameters. */
                if(!CheckParameter(jParams, "password", "string"))
                    throw Exception(-128, "-requirepassword active, must pass in password=<password> for all commands when enabled");

                /* Parse out password. */
                const SecureString strPassword =
                    SecureString(jParams["password"].get<std::string>().c_str());

                /* Check our password input compared to our internal sigchain password. */
                if(rSession.Credentials()->Password() != strPassword)
                    increment_failures(hashSession);
            }

            /* Get the active pin if not currently stored. */
            if(!rSession.Unlock(strPIN, nRequestedActions))
                strPIN = ExtractPIN(jParams);

            /* Check for crypto object register. */
            const TAO::Register::Address hashCrypto =
                TAO::Register::Address(std::string("crypto"), rSession.Genesis(), TAO::Register::Address::CRYPTO);

            /* Read the crypto object register. */
            TAO::Register::Object oCrypto;
            if(!LLD::Register->ReadObject(hashCrypto, oCrypto, TAO::Ledger::FLAGS::LOOKUP))
                increment_failures(hashSession);

            /* Read the key type from crypto object register. */
            const uint256_t hashAuth =
                oCrypto.get<uint256_t>("auth");

            /* Check if the auth has is deactivated. */
            if(hashAuth == 0)
                increment_failures(hashSession);

            /* Generate a key to check credentials against. */
            const uint256_t hashCheck =
                rSession.Credentials()->KeyHash("auth", 0, strPIN, hashAuth.GetType());

            /* Check for invalid authorization hash. */
            if(hashAuth != hashCheck)
                increment_failures(hashSession);

            /* Reset our activity and auth attempts if pin was successfully unlocked. */
            rSession.nAuthFailures = 0;
            rSession.nLastActive   = runtime::unifiedtimestamp();
        }

        /* Get bytes of our session. */
        const std::vector<uint8_t> vSession =
            hashSession.GetBytes();

        /* Get an xxHash. */
        const uint64_t nHash =
            XXH64(&vSession[0], vSession.size(), 0);

        return vLocks[nHash % vLocks.size()];
    }


    /* Terminate an active session by parameters. */
    void Authentication::Terminate(const encoding::json& jParams)
    {
        RECURSIVE(MUTEX);

        /* Get the current session-id. */
        const uint256_t hashSession =
            ExtractHash(jParams, "session", default_session());

        /* Terminate the session now. */
        terminate_session(hashSession);
    }


    /* Shuts down the current authentication systems. */
    void Authentication::Shutdown()
    {

    }


    /* Terminate an active session by parameters. */
    void Authentication::terminate_session(const uint256_t& hashSession)
    {
        /* Check for active session. */
        if(!mapSessions.count(hashSession))
            return;

        /* Erase the session from map. */
        mapSessions.erase(hashSession);
    }


    /* Increment the failure counter to deauthorize user after failed auth. */
    void Authentication::increment_failures(const uint256_t& hashSession)
    {
        /* Check for active session. */
        if(!mapSessions.count(hashSession))
            throw Exception(-9, "Session doesn't exist");

        /* Get a copy of our current active session. */
        const Session& rSession =
            mapSessions[hashSession];

        /* Increment the auth failures. */
        if(++rSession.nAuthFailures >= config::GetArg("-authattempts", 3))
        {
            /* Terminate our internal session. */
            terminate_session(hashSession);

            /* Return failure to API endpoint. */
            throw Exception(-290, "Too many invalid password/pin attempts (",
                rSession.nAuthFailures.load(), "): Session ", hashSession.SubString(), " Terminated");
        }

        throw Exception(-139, "Failed to unlock");
    }


    /* Checks for the correct session-id for single user mode. */
    uint256_t Authentication::default_session()
    {
        /* Handle for the default session. */
        if(!config::fMultiuser.load())
            return SESSION::DEFAULT;

        return ~uint256_t(0);
    }
}