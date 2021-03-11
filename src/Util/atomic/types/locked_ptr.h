/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#pragma once

#include <atomic/include/typedef.h>

namespace util::atomic
{
	/** lock_control
	 *
	 *  Track the current pointer references.
	 *
	 **/
	struct lock_control
	{
	    /** Recursive mutex for locking locked_ptr. **/
	    std::recursive_mutex MUTEX;


	    /** Reference counter for active copies. **/
	    util::atomic::uint32_t nCount;


	    /** Default Constructor. **/
	    lock_control( )
	    : MUTEX  ( )
	    , nCount (1)
	    {
	    }


	    /** count
	     *
	     *  Access atomic with easier syntax.
	     *
	     **/
	    uint32_t count()
	    {
	        return nCount.load();
	    }
	};


	/** lock_proxy
	 *
	 *  Temporary class that unlocks a mutex when outside of scope.
	 *  Useful for protecting member access to a raw pointer.
	 *
	 **/
	template <class TypeName>
	class lock_proxy
	{
	    /** Reference of the mutex. **/
	    std::recursive_mutex& MUTEX;


	    /** The pointer being locked. **/
	    TypeName* pData;


	public:

	    /** Basic constructor
	     *
	     *  Assign the pointer and reference to the mutex.
	     *
	     *  @param[in] pData The pointer to shallow copy
	     *  @param[in] MUTEX_IN The mutex reference
	     *
	     **/
	    lock_proxy(TypeName* pDataIn, std::recursive_mutex& MUTEX_IN)
	    : MUTEX (MUTEX_IN)
	    , pData (pDataIn)
	    {
	    }


	    /** Destructor
	    *
	    *  Unlock the mutex.
	    *
	    **/
	    ~lock_proxy()
	    {
	       MUTEX.unlock();
	    }


	    /** Member Access Operator.
	    *
	    *  Access the memory of the raw pointer.
	    *
	    **/
	    TypeName* operator->() const
	    {
	        return pData;
	    }
	};


	/** locked_ptr
	 *
	 *  Pointer with member access protected with a mutex.
	 *
	 **/
	template<class TypeName>
	class locked_ptr
	{
	    /** The internal locking mutex. **/
	    mutable lock_control* pRefs;


	    /** The internal raw poitner. **/
	    TypeName* pData;


	public:

	    /** Default Constructor. **/
	    locked_ptr()
	    : pRefs  (nullptr)
	    , pData  (nullptr)
	    {
	    }


	    /** Constructor for storing. **/
	    locked_ptr(TypeName* pDataIn)
	    : pRefs  (new lock_control())
	    , pData  (pDataIn)
	    {
	    }


	    /** Copy Constructor. **/
	    locked_ptr(const locked_ptr<TypeName>& ptrIn)
	    : pRefs (ptrIn.pRefs)
	    , pData (ptrIn.pData)
	    {
	        ++pRefs->nCount;

	        debug::log(0, FUNCTION, "New copy for reference ", pRefs->count());
	    }


	    /** Move Constructor. **/
	    locked_ptr(const locked_ptr<TypeName>&& ptrIn)
	    : pRefs (std::move(ptrIn.pRefs))
	    , pData (std::move(ptrIn.pData))
	    {
	    }


	    /** Destructor. **/
	    ~locked_ptr()
	    {
	        /* Adjust our reference count. */
	        if(pRefs->count() > 0)
	            --pRefs->nCount;

	        /* Delete if no more references. */
	        if(pRefs->count() == 0)
	        {
	            debug::log(0, FUNCTION, "Deleting copy for reference ", pRefs->count());

	            /* Cleanup the main raw pointer. */
	            if(pData)
	            {
	                delete pData;
	                pData = nullptr;
	            }

	            /* Cleanup the control block. */
	            if(pRefs)
	            {
	                delete pRefs;
	                pRefs = nullptr;
	            }
	        }
	    }


	    /** Assignment operator. **/
	    locked_ptr& operator=(const locked_ptr<TypeName>& ptrIn)
	    {
	        /* Shallow copy pointer and control block. */
	        pRefs  = ptrIn.pRefs;
	        pData  = ptrIn.pData;

	        /* Increase our reference count now. */
	        ++pRefs->nCount;

	        debug::log(0, FUNCTION, "New copy for reference ", pRefs->count());
	    }


	    /** Assignment operator. **/
	    locked_ptr& operator=(TypeName* pDataIn) = delete;


	    /** Equivilent operator.
	     *
	     *  @param[in] a The data type to compare to.
	     *
	     **/
	    bool operator==(const TypeName& pDataIn) const
	    {
	        RECURSIVE(pRefs->MUTEX);

	        /* Check for dereferencing nullptr. */
	        if(pData == nullptr)
	            throw std::range_error(debug::warning(FUNCTION, "dereferencing nullptr"));

	        return *pData == pDataIn;
	    }


	    /** Equivilent operator.
	     *
	     *  @param[in] a The data type to compare to.
	     *
	     **/
	    bool operator==(const TypeName* pDataIn) const
	    {
	        RECURSIVE(pRefs->MUTEX);

	        return pData == pDataIn;
	    }


	    /** Not equivilent operator.
	     *
	     *  @param[in] a The data type to compare to.
	     *
	     **/
	    bool operator!=(const TypeName* pDataIn) const
	    {
	        RECURSIVE(pRefs->MUTEX);

	        return pData != pDataIn;
	    }

	    /** Not operator
	     *
	     *  Check if the pointer is nullptr.
	     *
	     **/
	    bool operator!(void)
	    {
	        RECURSIVE(pRefs->MUTEX, pRefs->MUTEX);

	        return pData == nullptr;
	    }


	    /** Member access overload
	     *
	     *  Allow locked_ptr access like a normal pointer.
	     *
	     **/
	    lock_proxy<TypeName> operator->()
	    {
	        /* Lock our mutex before going forward. */
	        pRefs->MUTEX.lock();

	        return lock_proxy<TypeName>(pData, pRefs->MUTEX);
	    }


	    /** dereference operator overload
	     *
	     *  Load the object from memory.
	     *
	     **/
	    TypeName operator*() const
	    {
	        RECURSIVE(pRefs->MUTEX);

	        /* Check for dereferencing nullptr. */
	        if(pData == nullptr)
	            throw std::range_error(debug::warning(FUNCTION, "dereferencing nullptr"));

	        return *pData;
	    }
	};
}
