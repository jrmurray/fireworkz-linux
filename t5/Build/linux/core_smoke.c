/* core_smoke.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Smoke test for the Linux build of the platform-independent core */

#include "common/gflags.h"

#include "ob_skel/flags.h"

#include "cmodules/file.h"

#include "cmodules/monotime.h"

#include "cmodules/typepack.h"

#include "cmodules/ucs4.h"

#include <time.h>
#include <unistd.h>

static int n_failures = 0;

#define check(cond) \
    do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++n_failures; } } while(0)

static void
test_dates(void)
{
    SS_DATE_DATE ss_date_date;
    S32 year, month, day;

    (void) ss_ymd_to_dateval(&ss_date_date, 2026, 10, 31);
    check(status_ok(ss_dateval_to_ymd(ss_date_date, &year, &month, &day)));
    check((2026 == year) && (10 == month) && (31 == day));
}

static void
test_codepage(void)
{
    check(SBCHAR_CODEPAGE_WINDOWS_1252 == get_system_codepage());
    /* NB 0x80 maps to U+20A0 not U+20AC (EURO SIGN) in the upstream tables - not tested here */
    check(0x201CU == ucs4_from_sbchar_with_codepage(0x93, SBCHAR_CODEPAGE_WINDOWS_1252)); /* LEFT DOUBLE QUOTATION MARK */
    check(0x93U == ucs4_to_sbchar_try_with_codepage(0x201CU, SBCHAR_CODEPAGE_WINDOWS_1252));
    check(0xE9U == ucs4_from_sbchar_with_codepage(0xE9, SBCHAR_CODEPAGE_WINDOWS_1252)); /* e acute */
}

static void
test_filenames(void)
{
    check(file_is_rooted(TEXT("/home/user/letter.fwk")));
    check(!file_is_rooted(TEXT("letter.fwk")));
    check(0 == tstrcmp(file_leafname(TEXT("/home/user/letter.fwk")), TEXT("letter.fwk")));
}

static void
test_f64_layout(void)
{
    /* Fireworkz files store IEEE doubles little-endian (8087 layout) */
    static const BYTE one_8087[8] = { 0, 0, 0, 0, 0, 0, (BYTE) 0xF0, 0x3F };
    BYTE buffer[8];
    const F64 value = 3.14159;

    writeval_F64_as_8087(buffer, 1.0);
    check(0 == memcmp(buffer, one_8087, sizeof(buffer)));

    writeval_F64_as_8087(buffer, value);
    check(value == readval_F64_from_8087(buffer));
}

static void
test_file_io(void)
{
    static const char data[] = "Fireworkz on Linux\n";
    TCHARZ filename[BUF_MAX_PATHSTRING];
    FILE_HANDLE file_handle;
    char buffer[sizeof(data)];
    U32 bytesread = 0;

    (void) snprintf(filename, elemof32(filename), "/tmp/fireworkz_core_smoke_%d", (int) getpid());

    check(status_ok(t5_file_open(filename, file_open_write, &file_handle, TRUE)));
    check(status_ok(file_write_bytes(data, sizeof32(data), file_handle)));
    check(status_ok(t5_file_close(&file_handle)));

    check(file_is_file(filename));
    check(!file_is_dir(filename));

    check(status_ok(t5_file_open(filename, file_open_read, &file_handle, TRUE)));
    check(status_ok(file_read_bytes(buffer, sizeof32(buffer), &bytesread, file_handle)));
    check(sizeof32(data) == bytesread);
    check(0 == memcmp(buffer, data, sizeof(data)));
    check(status_ok(t5_file_close(&file_handle)));

    check(status_ok(file_remove(filename)));
    check(!file_is_file(filename));
}

static void
test_monotime(void)
{
    const MONOTIME t0 = monotime();
    struct timespec ts = { 0, 20 * 1000 * 1000 };

    (void) nanosleep(&ts, NULL);

    check(monotime_difftimes(monotime(), t0) >= MONOTIMEDIFF_VALUE_FROM_MS(15));
}

int
main(void)
{
    test_dates();
    test_codepage();
    test_filenames();
    test_f64_layout();
    test_file_io();
    test_monotime();

    if(0 != n_failures)
    {
        fprintf(stderr, "core_smoke: %d failure(s)\n", n_failures);
        return(EXIT_FAILURE);
    }

    printf("core_smoke: all checks passed\n");
    return(EXIT_SUCCESS);
}

/* end of core_smoke.c */
