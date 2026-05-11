#define PNL_IMPLEMENTATION
#include "c-tuikit/c-tuikit.h"

static void noop_action(void *ctx, pnl_app_t *app)
{
    (void)ctx;
    (void)app;
}

int main(void)
{
    pnl_app_t *app = NULL;
    int key = PNL_KEY_NONE;
    int status;

    status = pnl_app_create("demo", NULL, &app);
    if (status == PNL_OK)
        status = pnl_app_on_status(app, NULL, NULL);
    if (status == PNL_OK)
        status = pnl_app_add_action(app, "First", noop_action, NULL);
    if (status == PNL_OK && pnl_status_line(app, PNL_MARK_OK, NULL, "x") != PNL_ERR_ARG)
        status = PNL_ERR_ARG;
    if (status == PNL_OK && pnl_result(app, PNL_MARK_OK, "x") != PNL_ERR_ARG)
        status = PNL_ERR_ARG;
    if (status == PNL_OK && pnl_app_run(app, 0, NULL, NULL) != PNL_OK)
        status = PNL_ERR_ARG;
    if (pnl_key_decode((const unsigned char *)"\x1b[A", 3, &key) != 3 || key != PNL_KEY_UP)
        status = PNL_ERR_ARG;
    if (pnl_strerror(status) == 0)
        status = PNL_ERR_ARG;
    pnl_app_destroy(app);
    return status == PNL_OK ? 0 : 1;
}
