
#if defined(HAVE_CONFIG_H)
#include "config_ac.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "test_chansrv.h"

static int
run_suite(const char *test_name)
{
    const char *env = getenv("TEST_NAME");
    return (env == NULL || strcmp(env, test_name) == 0);
}

int main (void)
{
    int number_failed;
    SRunner *sr;

    sr = srunner_create (NULL);
    if (run_suite("clipboard_dib"))
    {
        srunner_add_suite(sr, make_suite_test_clipboard_dib());
    }

    srunner_set_tap(sr, "-");
    srunner_run_all (sr, CK_ENV);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
