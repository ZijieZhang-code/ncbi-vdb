/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include <cloud/extern.h>
#include <kfc/refcount-impl.h>

#include <klib/rc.h>
#include <klib/status.h>
#include <cloud/aws.h>
#include <cloud/gcp.h>

#include "cloud-priv.h"

#include <atomic.h>
#include <assert.h>

/*--------------------------------------------------------------------------
 * CloudMgr
 */

static atomic_ptr_t cloud_singleton;

/* Whack
 */
static
rc_t CloudMgrWhack ( CloudMgr * self )
{
    CloudMgr * our_mgr = atomic_read_ptr ( & cloud_singleton );
    if ( self != our_mgr )
    {
        CloudRelease ( self -> cur );
#ifdef _h_cloud_aws_
        AWSRelease ( self -> aws );
#endif
#ifdef _h_cloud_gcp_
        GCPRelease ( self -> gcp );
#endif
#ifdef _h_cloud_azure_
        AzureRelease ( self -> azure );
#endif
        KNSManagerRelease ( self -> kns );
        KConfigRelease ( self -> kfg );
        free ( self );
    }
    return 0;
}

static
void CloudMgrDetermineCurrentCloud ( CloudMgr * self )
{
#ifdef _h_cloud_aws_
    if ( CloudMgrWithinAWS ( self ) )
    {
        self -> cur_id = cloud_provider_aws;
        return;
    }
#endif

#ifdef _h_cloud_gcp_
    if ( CloudMgrWithinGCP ( self ) )
    {
        self -> cur_id = cloud_provider_gcp;
        return;
    }
#endif

#ifdef _h_cloud_azure_
    if ( CloudMgrWithinAzure ( self ) )
    {
        self -> cur_id = cloud_provider_azure;
        return;
    }
#endif

    self -> cur_id = cloud_provider_none;
}

static rc_t CloudMgrInit(CloudMgr ** mgrp, const KConfig * kfg,
    const KNSManager * kns, CloudProviderId provider)
{
    rc_t rc = 0;

    CloudMgr * our_mgr = NULL;

    our_mgr = calloc(1, sizeof * our_mgr);
    if (our_mgr == NULL)
        rc = RC(rcCloud, rcMgr, rcAllocating, rcMemory, rcExhausted);
    else
    {
        /* convert allocation into a ref-counted object */
        KRefcountInit(&our_mgr->refcount, 1, "CloudMgr", "init", "cloud");

        if (kfg == NULL)
            /* make KConfig if it was not provided */
            rc = KConfigMake((KConfig **)& kfg, NULL);
        else
            /* attach reference to KConfig */
            rc = KConfigAddRef(kfg);

        if (rc == 0)
        {
            our_mgr->kfg = kfg;

            /* attach reference to KNSManager */
            if (kns == NULL)
                rc = KNSManagerMake((KNSManager **)& kns);
            else
                rc = KNSManagerAddRef(kns);
            if (rc == 0)
                our_mgr->kns = kns;

            if (rc == 0)
            {
                if (provider == cloud_provider_none)
                /* examine environment for current cloud */
                    CloudMgrDetermineCurrentCloud(our_mgr);
                else
                    our_mgr->cur_id = provider;

                /* if within a cloud, initialize */
                if (our_mgr->cur_id != cloud_provider_none)
                    rc = CloudMgrMakeCloud(our_mgr, &our_mgr->cur, our_mgr->cur_id);
            }
        }

        if (rc == 0) {
            assert(mgrp);
            *mgrp = our_mgr;
        }
    }

    return rc;
}

/* Make
 *  this is a singleton
 */
LIB_EXPORT rc_t CC CloudMgrMake ( CloudMgr ** mgrp,
    const KConfig * kfg, const KNSManager * kns )
{
    rc_t rc = 0;

    if ( mgrp == NULL )
        rc = RC ( rcCloud, rcMgr, rcAllocating, rcParam, rcNull );
    else
    {
        {
            CloudMgr * our_mgr = NULL;

            /* grab single-shot singleton */
            our_mgr = atomic_read_ptr ( & cloud_singleton );
            if ( our_mgr != NULL )
            {
            singleton_exists:

                /* add a new reference and return */
                rc = CloudMgrAddRef ( our_mgr );
                if ( rc != 0 )
                    our_mgr = NULL;

                * mgrp = our_mgr;
                return rc;
            }

            /* singleton was NULL. call CloudMgrInit to make it from scratch. */
            rc = CloudMgrInit(&our_mgr, kfg, kns, cloud_provider_none);
            /* if everything looks good... */
            if ( rc == 0 )
            {
                /* try to set single-shot ( set once, never reset ) */
                CloudMgr * new_mgr = atomic_test_and_set_ptr (
                    & cloud_singleton, our_mgr, NULL );

                /* if "new_mgr" is not NULL,
                   then some other thread beat us to it */
                if ( new_mgr != NULL )
                {
                    /* test logic */
                    assert ( our_mgr != new_mgr );

                    /* not an error condition - douse our version */
                    CloudMgrWhack ( our_mgr );

                    /* use the other thread's version
                       use common code
                       even if it means a "goto" in this case */
                    our_mgr = new_mgr;
                    goto singleton_exists;
                }

                /* arriving here means success */
                * mgrp = our_mgr;
                return rc;
            }

            CloudMgrWhack ( our_mgr );
        }

        * mgrp = NULL;
    }

    return rc;
}

LIB_EXPORT rc_t CC CloudMgrMakeWithProvider ( CloudMgr ** mgrp, CloudProviderId provider )
{
    CloudMgr * self = NULL;
    rc_t rc = CloudMgrInit(&self, NULL, NULL, provider);
    if ( rc == 0 )
    {
        * mgrp = self;
        return 0;
    }

    * mgrp = NULL;
    return rc;
}


/* AddRef
 * Release
 */
LIB_EXPORT rc_t CC CloudMgrAddRef ( const CloudMgr * self )
{
    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "CloudMgr" ) )
        {
        case krefLimit:
            return RC ( rcCloud, rcMgr, rcAttaching, rcRange, rcExcessive );
        case krefNegative:
            return RC ( rcCloud, rcMgr, rcAttaching, rcSelf, rcInvalid );
        default:
            break;
        }
    }
    return 0;
}

LIB_EXPORT rc_t CC CloudMgrRelease ( const CloudMgr * self )
{
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "CloudMgr" ) )
        {
        case krefWhack:
            return CloudMgrWhack ( ( CloudMgr * ) self );
        case krefNegative:
            return RC ( rcCloud, rcMgr, rcReleasing, rcRange, rcExcessive );
        default:
            break;
        }
    }

    return 0;
}

/* CurrentProvider
 *  ask whether we are currently executing within a cloud
 */
LIB_EXPORT rc_t CC CloudMgrCurrentProvider ( const CloudMgr * self, CloudProviderId * cloud_provider )
{
    rc_t rc;

    if ( cloud_provider == NULL )
        rc = RC ( rcCloud, rcMgr, rcAccessing, rcParam, rcNull );
    else
    {
        if ( self == NULL )
            rc = RC ( rcCloud, rcMgr, rcAccessing, rcSelf, rcNull );
        else
        {
            * cloud_provider = self -> cur_id;
            return 0;
        }

        * cloud_provider = cloud_provider_none;
    }

    return rc;
}

/* MakeCloud
 * MakeCurrentCloud
 */
LIB_EXPORT rc_t CC CloudMgrMakeCloud ( CloudMgr * self, Cloud ** cloud, CloudProviderId cloud_provider )
{
    rc_t rc = 0;

    /* check return parameter */
    if ( cloud == NULL )
        rc = RC ( rcCloud, rcMgr, rcAllocating, rcParam, rcNull );
    else
    {
        /* check input parameters */
        if ( self == NULL )
            rc = RC ( rcCloud, rcMgr, rcAllocating, rcSelf, rcNull );
        else if ( cloud_provider == cloud_provider_none ||
                  cloud_provider >= cloud_num_providers )
            rc = RC ( rcCloud, rcMgr, rcAllocating, rcParam, rcInvalid );
        else
        {
            /* look for cached Cloud */
            switch ( cloud_provider )
            {
            case cloud_provider_aws:
                if ( self -> aws != NULL )
                    return AWSToCloud ( self -> aws, cloud );
                break;
            case cloud_provider_gcp:
                if ( self -> gcp != NULL )
                    return GCPToCloud ( self -> gcp, cloud );
                break;
            default:
                break;
            }

            /* create a Cloud object via selection matrix:

               where\target | aws | gcp | azure
               -------------+-----+-----+------
                    outside |  x  |  x  |  x
                     in aws |  x  |     |
                     in gcp |     |  x  |
                   in azure |     |     |  x
               -------------+-----+-----+------

               this may be relaxed in the future, but for today
               it's hard coded that from within any cloud there is
               only access to the same cloud allowed. */

            switch ( cloud_provider * cloud_num_providers + self -> cur_id )
            {
#define CASE( a, b ) \
    case ( a ) * cloud_num_providers + ( b )

#ifdef _h_cloud_aws_
            /* asking for AWS */
            CASE ( cloud_provider_aws, cloud_provider_none ):
            CASE ( cloud_provider_aws, cloud_provider_aws ):
            {
                assert ( self -> aws == NULL );
                rc = CloudMgrMakeAWS ( self, & self -> aws );
                if ( rc == 0 )
                    return AWSToCloud ( self -> aws, cloud );
                break;
            }
#if ALLOW_EXT_CLOUD_ACCESS
            CASE ( cloud_provider_aws, cloud_provider_gcp ):
            CASE ( cloud_provider_aws, cloud_provider_azure ):
#error "this should require a special class"
#endif
#endif

#ifdef _h_cloud_gcp_
            /* asking for GCP */
            CASE ( cloud_provider_gcp, cloud_provider_none ):
            CASE ( cloud_provider_gcp, cloud_provider_gcp ):
            {
                assert ( self -> gcp == NULL );
                rc = CloudMgrMakeGCP ( self, & self -> gcp );
                if ( rc == 0 )
                    return GCPToCloud ( self -> gcp, cloud );
                break;
            }
#if ALLOW_EXT_CLOUD_ACCESS
            CASE ( cloud_provider_gcp, cloud_provider_aws ):
            CASE ( cloud_provider_gcp, cloud_provider_azure ):
#error "this should require a special class"
#endif
#endif

#ifdef _h_cloud_azure_
            /* asking for Azure */
            CASE ( cloud_provider_azure, cloud_provider_none ):
            CASE ( cloud_provider_azure, cloud_provider_azure ):
#error "not implemented"
#if ALLOW_EXT_CLOUD_ACCESS
            CASE ( cloud_provider_azure, cloud_provider_aws ):
            CASE ( cloud_provider_azure, cloud_provider_gcp ):
#error "this should require a special class"
#endif
#endif
            default:
                ( void ) 0;
#undef CASE
            }
        }

        * cloud = NULL;
    }

    return rc;
}

LIB_EXPORT rc_t CC CloudMgrGetCurrentCloud ( const CloudMgr * self, Cloud ** cloud )
{
    rc_t rc;

    if ( cloud == NULL )
        rc = RC ( rcCloud, rcMgr, rcAccessing, rcParam, rcNull );
    else
    {
        if ( self == NULL )
            rc = RC ( rcCloud, rcMgr, rcAccessing, rcSelf, rcNull );
        else if ( self -> cur_id == cloud_provider_none )
            rc = RC ( rcCloud, rcMgr, rcAccessing, rcCloudProvider, rcNotFound );
        else
        {
            rc = CloudAddRef ( self -> cur );
            if ( rc == 0 )
            {
                * cloud = self -> cur;
                return 0;
            }
        }

        * cloud = NULL;
    }
    return rc;
}
