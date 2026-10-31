/* ho_main.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* The program's entry point on Linux: the counterpart of windows/winmain.c.
 *
 * Start up the skeleton, make a first document, and run the event loop until the last
 * window has closed. */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_file/xp_file.h"

#include "ob_skel/linux/qt_host.h"

#include <locale.h>
#include <setjmp.h>
#include <time.h>

/* declared for Windows only in the skeleton's header */

_Check_return_
extern BOOL
some_document_windows(void);

extern void
t5_do_exit(void);

/* the Windows names that the program's globals have */

/*extern*/ int g_nCmdShow;

static BOOL event_loop_jmp_set = FALSE;
static jmp_buf event_loop_jmp_buf;

extern void
host_longjmp_to_event_loop(int val)
{
    if(event_loop_jmp_set)
        longjmp(event_loop_jmp_buf, val);
}

/* Where to look for resources: messages and bitmaps generated at build time, the Unicode data,
 * and the language-independent files, or a path given in FIREWORKZ_RESOURCES */

#if !defined(FIREWORKZ_DEFAULT_TEMPLATES)
#define FIREWORKZ_DEFAULT_TEMPLATES "/usr/share/fireworkz/Templates"
#endif

#if !defined(FIREWORKZ_DEFAULT_RESOURCES)
#define FIREWORKZ_DEFAULT_RESOURCES "/usr/share/fireworkz/Resources"
#endif

extern void
host_initialise_file_paths(void)
{
    const char * p_env = getenv("FIREWORKZ_RESOURCES");
    const PCTSTR resources_path = (NULL != p_env) ? p_env : TEXT(FIREWORKZ_DEFAULT_RESOURCES);

    status_assert(file_path_set(resources_path, FILE_PATH_RESOURCES));

    /* the user's own data, such as Choices, goes in their configuration directory */
    {
    TCHARZ user_path[BUF_MAX_PATHSTRING];
    const char * p_xdg = getenv("XDG_CONFIG_HOME");
    const char * p_home = getenv("HOME");

    if((NULL != p_xdg) && (CH_NULL != p_xdg[0]))
        consume_int(snprintf(user_path, elemof32(user_path), "%s/fireworkz/", p_xdg));
    else if(NULL != p_home)
        consume_int(snprintf(user_path, elemof32(user_path), "%s/.config/fireworkz/", p_home));
    else
        user_path[0] = CH_NULL;

    if(CH_NULL != user_path[0])
        status_assert(file_path_set(user_path, FILE_PATH_USER));
    } /*block*/

    /* templates: the user's own first, then the ones that come with the program */
    {
    TCHARZ templates_path[BUF_MAX_PATHSTRING * 2];
    const char * p_env_templates = getenv("FIREWORKZ_TEMPLATES");
    const char * p_xdg_data = getenv("XDG_DATA_HOME");
    const char * p_home = getenv("HOME");
    TCHARZ user_templates[BUF_MAX_PATHSTRING];

    if((NULL != p_xdg_data) && (CH_NULL != p_xdg_data[0]))
        consume_int(snprintf(user_templates, elemof32(user_templates), "%s/fireworkz/" TEMPLATES_SUBDIR, p_xdg_data));
    else if(NULL != p_home)
        consume_int(snprintf(user_templates, elemof32(user_templates), "%s/.local/share/fireworkz/" TEMPLATES_SUBDIR, p_home));
    else
        user_templates[0] = CH_NULL;

    consume_int(snprintf(templates_path, elemof32(templates_path), "%s%s%s",
                         user_templates,
                         (CH_NULL != user_templates[0]) ? FILE_PATH_SEP_TSTR : "",
                         (NULL != p_env_templates) ? p_env_templates : TEXT(FIREWORKZ_DEFAULT_TEMPLATES)));

    status_assert(file_path_set(templates_path, FILE_PATH_TEMPLATES));
    } /*block*/

    file_build_paths();
}

_Check_return_
static BOOL
main_init(
    _InVal_     int argc,
    _In_        char * argv[])
{
    UNREFERENCED_PARAMETER_InVal_(argc);
    UNREFERENCED_PARAMETER(argv);

    /* set locale for isalpha etc. (ctype.h functions) */
    consume_ptr(setlocale(LC_CTYPE, "C")); /* standard ASCII NB NOT "ISO8859-1" (ISO Latin-1) */
    consume_ptr(setlocale(LC_TIME,  "C"));
    tzset();

    if(status_fail(startup_t5_application_1()))
        return(FALSE);

    status_consume(aligator_init());

    file_startup();

    host_initialise_file_paths();

    /* Make error messages available for startup */
    resource_startup();

    status_assert(resource_init(OBJECT_ID_SKEL, LOAD_MESSAGES_FILE, LOAD_RESOURCES));

    /* Startup the application. Any errors will have been reported */
    if(status_fail(startup_t5_application_2()))
        return(FALSE);

    /* TODO: command line options and files to open */

    /* If nothing has been loaded, open a document made from the default template */
    if(!some_document_windows())
        status_assert(object_call_id(OBJECT_ID_SKEL, P_DOCU_NONE, T5_CMD_NEW_DOCUMENT_DEFAULT, P_DATA_NONE));

    return(TRUE);
}

extern int
main(
    int argc,
    char * argv[])
{
    BOOL looping = TRUE;

    if(!main_init(argc, argv))
        goto fallout;

    /* set up a point we can longjmp back to on serious error */
    switch(setjmp(event_loop_jmp_buf))
    {
    default:
        /* returned here from exception - tidy up as necessary */
        status_assert(maeve_service_event(P_DOCU_NONE, T5_MSG_HAD_SERIOUS_ERROR, P_DATA_NONE));
        break;

    case 0:
        event_loop_jmp_set = TRUE;
        break;
    }

    if(!some_document_windows())
        goto fallout;

    while(looping)
    {
        WM_EVENT res = wm_event_get(FALSE /*fgNullEventsWanted*/);

        if(WM_EVENT_PROCESSED != res)
            break;
    }

fallout:
    looping = FALSE; /* don't run the event loop if we get a longjmp() back above during closedown */

    docno_close_all();

    t5_do_exit();

    return(EXIT_SUCCESS);
}

/* end of ho_main.c */
