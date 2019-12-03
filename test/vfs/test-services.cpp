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
* ==============================================================================
*
* tests of names service
*/

#include <klib/debug.h> /* KDbgSetString */

#include <ktst/unit_test.hpp> /* KMain */

#include <vfs/path.h> /* VPathRelease */
#include <vfs/services.h> /* KServiceRelease */

#include <climits> /* PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

using std::string;

TEST_SUITE ( TestServices );

#define ALL

#ifdef ALL
TEST_CASE ( TestKServiceAddId ) {
    KService * s = NULL;

    REQUIRE_RC_FAIL ( KServiceAddId ( s, "0" ) );

    REQUIRE_RC ( KServiceMake ( & s ) );

    REQUIRE_RC_FAIL ( KServiceAddId ( s, NULL ) );
    REQUIRE_RC_FAIL ( KServiceAddId ( s, "" ) );

    for ( int i = 0; i < 512; ++ i )
        REQUIRE_RC ( KServiceAddId ( s, "0" ) );
    REQUIRE_RC ( KServiceAddId ( s, "0" ) );
    for ( int i = 0; i < 512; ++ i )
        REQUIRE_RC ( KServiceAddId ( s, "0" ) );

    REQUIRE_RC ( KServiceRelease ( s ) );
}
#endif

#ifdef ALL
TEST_CASE(TestKSrvResponseGetLocation) {
    KService * s = NULL;
    REQUIRE_RC(KServiceMake(&s));
    REQUIRE_RC(KServiceAddId(s, "SRR850901"));
    REQUIRE_RC(KServiceSetFormat(s, "all"));
    const KSrvResponse * r = NULL;
    REQUIRE_RC(KServiceNamesQuery(s, 0, &r));
    REQUIRE_RC_FAIL(KSrvResponseGetLocation(0, 0, 0, 0, 0, 0, 0));
    REQUIRE_RC_FAIL(KSrvResponseGetLocation(r, 0, 0, 0, 0, 0, 0));
    REQUIRE_RC_FAIL(KSrvResponseGetLocation(r, "SRR850901", 0, 0, 0, 0, 0));
    REQUIRE_RC_FAIL(KSrvResponseGetLocation(r, 0, "SRR850901", 0, 0, 0, 0));
    REQUIRE_RC(KSrvResponseGetLocation(r, "SRR850901", "SRR850901",
        0, 0, 0, 0));

    const VPath * local = NULL;
    REQUIRE_RC(KSrvResponseGetLocation(r, "SRR850901", "SRR850901",
        &local, 0, 0, 0));
    REQUIRE_NULL(local);

    rc_t rcLocal = 0;
    REQUIRE_RC_FAIL(KSrvResponseGetLocation(r, "SRR850901", "SRR850901",
        0, &rcLocal, 0, 0));
    REQUIRE_RC(KSrvResponseGetLocation(r, "SRR850901", "SRR850901",
        &local, &rcLocal, 0, 0));
    REQUIRE_NULL(local);
    REQUIRE_RC_FAIL(rcLocal);

    REQUIRE_RC(KSrvResponseGetLocation(r, "SRR850901", "SRR850901.vdbcache",
        &local, &rcLocal, 0, 0));
    REQUIRE_RC(rcLocal);
    char buffer[PATH_MAX] = "";
    REQUIRE_RC(VPathReadPath(local, buffer, sizeof buffer, NULL));
    REQUIRE_EQ(string(buffer),
        string("/netmnt/traces04/sra11/SRR/000830/SRR850901.vdbcache"));
    REQUIRE_RC(VPathRelease(local));

    const VPath * cache = NULL;
    rc_t rcCache = 0;

    REQUIRE_RC_FAIL(KSrvResponseGetLocation(r, "SRR850901", "SRR850901.qq",
        &local, &rcLocal, &cache, &rcCache));
    REQUIRE_NULL(local);
    REQUIRE_NULL(cache);
    REQUIRE_RC(rcLocal);
    REQUIRE_RC(rcCache);

    REQUIRE_RC_FAIL(KSrvResponseGetLocation(r, "SRR000001", "SRR000001",
        &local, &rcLocal, &cache, &rcCache));
    REQUIRE_NULL(local);
    REQUIRE_NULL(cache);
    REQUIRE_RC(rcLocal);
    REQUIRE_RC(rcCache);

    REQUIRE_RC(KSrvResponseRelease(r));
    REQUIRE_RC(KServiceRelease(s));
}
#endif

extern "C" {
    ver_t CC KAppVersion ( void ) { return 0; }

    rc_t CC KMain ( int argc, char * argv [] ) {

        if (
0) assert(!KDbgSetString("VFS"));

        return TestServices ( argc, argv );
    }
}
