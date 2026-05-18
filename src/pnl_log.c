#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define PNL_LOG_TEXT 240
#define PNL_LOG_AVERAGE 128
#define PNL_LOG_FILTER_MAX 128
#define PNL_LOG_FOOTER "up/down: scroll  end: follow  v: level  /: filter  q: back"
#define PNL_LOG_FILTER_FOOTER "type to filter  enter: keep  esc: clear"

struct pnl_log_entry {
    unsigned long long seq;
    time_t when;
    unsigned int start;
    unsigned char level;
};

struct pnl_log {
    pnl_app_t *app;
    struct pnl_lock lock;
    struct pnl_log_entry *entries;
    size_t capacity;
    char *text;
    size_t text_size;
    size_t head;
    unsigned long long first_seq;
    unsigned long long next_seq;
    int pending;
};

struct pnl_log_view {
    pnl_log_t *logbook;
    size_t *shown;
    size_t shown_len;
    unsigned long long anchor;
    size_t page;
    int follow;
    int level;
    char filter[PNL_LOG_FILTER_MAX];
    struct pnl_edit edit;
    int editing;
    const char *footer;
};

static const char *const pnl_level_names[] = {"debug", "info", "warn", "error"};
static const int pnl_level_roles[] = {PNL_ROLE_MUTED, PNL_ROLE_INFO, PNL_ROLE_WARN, PNL_ROLE_FAIL};

int pnl_log_create(pnl_app_t *app, size_t capacity, pnl_log_t **out_log)
{
    pnl_log_t *logbook;
    int status;

    if (out_log != NULL)
        *out_log = NULL;
    if (app == NULL || capacity == 0 || capacity > ((size_t)-1) / 2 / sizeof(struct pnl_log_entry) ||
        capacity > ((unsigned int)-1 - PNL_LOG_TEXT) / PNL_LOG_AVERAGE || out_log == NULL)
        return PNL_ERR_ARG;
    status = pnl_term_wake_open(&app->term);
    if (status != PNL_OK)
        return status;
    logbook = pnl_mem_alloc(&app->mem, sizeof(*logbook));
    if (logbook == NULL)
        return PNL_ERR_NOMEM;
    memset(logbook, 0, sizeof(*logbook));
    logbook->app = app;
    logbook->capacity = capacity;
    logbook->text_size = capacity * PNL_LOG_AVERAGE + PNL_LOG_TEXT;
    logbook->entries = pnl_mem_alloc(&app->mem, capacity * sizeof(*logbook->entries));
    logbook->text = pnl_mem_alloc(&app->mem, logbook->text_size);
    status = logbook->entries == NULL || logbook->text == NULL ? PNL_ERR_NOMEM : pnl_lock_init(&logbook->lock);
    if (status != PNL_OK) {
        pnl_mem_free(&app->mem, logbook->entries);
        pnl_mem_free(&app->mem, logbook->text);
        pnl_mem_free(&app->mem, logbook);
        return status;
    }
    *out_log = logbook;
    return PNL_OK;
}

void pnl_log_destroy(pnl_log_t *logbook)
{
    if (logbook == NULL)
        return;
    pnl_lock_destroy(&logbook->lock);
    pnl_mem_free(&logbook->app->mem, logbook->entries);
    pnl_mem_free(&logbook->app->mem, logbook->text);
    pnl_mem_free(&logbook->app->mem, logbook);
}

static const char *pnl_log_text(const pnl_log_t *logbook, const struct pnl_log_entry *entry)
{
    return logbook->text + entry->start;
}

static size_t pnl_log_length(const char *text)
{
    size_t len = 0;

    while (text[len] != '\0' && text[len] != '\n' && len + 1 < PNL_LOG_TEXT)
        len++;
    while (len > 0 && text[len] != '\0' && text[len] != '\n' && ((unsigned char)text[len] & 0xc0) == 0x80)
        len--;
    return len;
}

static void pnl_log_evict(pnl_log_t *logbook, size_t reach)
{
    while (logbook->first_seq < logbook->next_seq) {
        const struct pnl_log_entry *oldest = &logbook->entries[logbook->first_seq % logbook->capacity];
        size_t ahead = (oldest->start + logbook->text_size - logbook->head) % logbook->text_size;

        if (logbook->next_seq - logbook->first_seq < logbook->capacity && ahead >= reach)
            return;
        logbook->first_seq++;
    }
}

static void pnl_log_store(pnl_log_t *logbook, struct pnl_log_entry *entry, const char *text)
{
    size_t len = pnl_log_length(text);
    size_t start = logbook->head + len + 1 > logbook->text_size ? 0 : logbook->head;
    size_t i;

    pnl_log_evict(logbook, start == 0 ? logbook->text_size - logbook->head + len + 1 : len + 1);
    for (i = 0; i < len; i++)
        logbook->text[start + i] = text[i] == '\t' ? ' ' : text[i];
    logbook->text[start + len] = '\0';
    entry->start = (unsigned int)start;
    logbook->head = (start + len + 1) % logbook->text_size;
}

int pnl_log_write(pnl_log_t *logbook, int level, const char *text)
{
    struct pnl_log_entry *entry;
    int wake;

    if (logbook == NULL || text == NULL || level < PNL_LEVEL_DEBUG || level > PNL_LEVEL_ERROR)
        return PNL_ERR_ARG;
    pnl_lock(&logbook->lock);
    pnl_log_evict(logbook, 0);
    entry = &logbook->entries[logbook->next_seq % logbook->capacity];
    pnl_log_store(logbook, entry, text);
    entry->seq = logbook->next_seq++;
    entry->when = time(NULL);
    entry->level = (unsigned char)level;
    wake = !logbook->pending;
    logbook->pending = 1;
    pnl_unlock(&logbook->lock);
    if (wake)
        pnl_term_wake(&logbook->app->term);
    return PNL_OK;
}

static unsigned long long pnl_log_first(const pnl_log_t *logbook)
{
    return logbook->first_seq;
}

static int pnl_log_matches(const struct pnl_log_view *view, const struct pnl_log_entry *entry)
{
    return entry->level >= view->level && pnl_text_contains(pnl_log_text(view->logbook, entry), view->filter);
}

static void pnl_log_collect(struct pnl_log_view *view)
{
    pnl_log_t *logbook = view->logbook;
    unsigned long long seq;

    view->shown_len = 0;
    for (seq = pnl_log_first(logbook); seq < logbook->next_seq; seq++) {
        size_t slot = (size_t)(seq % logbook->capacity);

        if (pnl_log_matches(view, &logbook->entries[slot]))
            view->shown[view->shown_len++] = slot;
    }
}

static size_t pnl_log_anchor_index(const struct pnl_log_view *view)
{
    size_t i = view->shown_len;

    while (i > 0 && view->logbook->entries[view->shown[i - 1]].seq > view->anchor)
        i--;
    return i;
}

static void pnl_log_clock(char *out, size_t out_len, time_t when)
{
    struct tm parts;

#if defined(_WIN32)
    localtime_s(&parts, &when);
#else
    localtime_r(&when, &parts);
#endif
    snprintf(out, out_len, "%02d:%02d:%02d", parts.tm_hour, parts.tm_min, parts.tm_sec);
}

static void pnl_log_draw_entry(pnl_canvas_t *canvas, const pnl_log_t *logbook, const struct pnl_log_entry *entry,
                               int y, const char *filter)
{
    const char *text = pnl_log_text(logbook, entry);
    char clock[16];
    const char *hit = NULL;
    const char *at;

    pnl_log_clock(clock, sizeof(clock), entry->when);
    pnl_canvas_text(canvas, 1, y, PNL_FG(PNL_ROLE_MUTED), clock);
    pnl_canvas_text(canvas, 11, y, PNL_FG(pnl_level_roles[entry->level]), pnl_level_names[entry->level]);
    pnl_canvas_text(canvas, 18, y, entry->level == PNL_LEVEL_DEBUG ? PNL_FG(PNL_ROLE_MUTED) : 0, text);
    for (at = text; filter[0] != '\0' && *at != '\0' && hit == NULL; at++)
        hit = pnl_text_contains_at(at, filter) ? at : NULL;
    if (hit != NULL) {
        char before[PNL_LOG_TEXT];
        char match[PNL_LOG_FILTER_MAX];

        snprintf(before, sizeof(before), "%.*s", (int)(hit - text), text);
        snprintf(match, sizeof(match), "%.*s", (int)strlen(filter), hit);
        pnl_canvas_text(canvas, 18 + pnl_text_width(before), y, PNL_REVERSE, match);
    }
}

static void pnl_log_draw_state(struct pnl_log_view *view, pnl_canvas_t *canvas, size_t end)
{
    char state[96];
    int width = pnl_canvas_width(canvas);
    size_t waiting = view->shown_len - end;

    if (view->follow)
        snprintf(state, sizeof(state), "following  %s+", pnl_level_names[view->level]);
    else
        snprintf(state, sizeof(state), "paused, %zu new  %s+", waiting, pnl_level_names[view->level]);
    pnl_canvas_text(canvas, width - pnl_text_width(state) - 1, 0, PNL_FG(view->follow ? PNL_ROLE_OK : PNL_ROLE_WARN),
                    state);
    if (!view->editing && view->filter[0] == '\0')
        return;
    pnl_canvas_text(canvas, 1, 0, PNL_FG(PNL_ROLE_ACCENT), "/");
    pnl_edit_draw(&view->edit, canvas, 3, 0, width - pnl_text_width(state) - 6, view->editing);
}

static void pnl_log_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    struct pnl_log_view *view = ctx;
    pnl_log_t *logbook = view->logbook;
    int rows = pnl_canvas_height(canvas) - 2;
    size_t end;
    size_t start;
    size_t i;

    (void)app;
    view->page = rows > 1 ? (size_t)rows : 1;
    pnl_lock(&logbook->lock);
    logbook->pending = 0;
    pnl_log_collect(view);
    end = view->follow ? view->shown_len : pnl_log_anchor_index(view);
    if (end < view->page)
        end = view->shown_len < view->page ? view->shown_len : view->page;
    start = rows > 0 && end > (size_t)rows ? end - (size_t)rows : 0;
    pnl_log_draw_state(view, canvas, end);
    for (i = start; i < end; i++)
        pnl_log_draw_entry(canvas, logbook, &logbook->entries[view->shown[i]], 2 + (int)(i - start), view->filter);
    if (view->shown_len == 0)
        pnl_canvas_text(canvas, 1, 2, PNL_FG(PNL_ROLE_MUTED), logbook->next_seq == 0 ? "nothing yet" : "no match");
    pnl_unlock(&logbook->lock);
}

static void pnl_log_scroll(struct pnl_log_view *view, size_t height, int key)
{
    size_t step = key == PNL_KEY_PAGE_UP || key == PNL_KEY_PAGE_DOWN ? height : 1;
    size_t end;

    pnl_lock(&view->logbook->lock);
    pnl_log_collect(view);
    end = view->follow ? view->shown_len : pnl_log_anchor_index(view);
    if (key == PNL_KEY_HOME)
        end = view->shown_len < height ? view->shown_len : height;
    else if (key == PNL_KEY_UP || key == 'k' || key == PNL_KEY_PAGE_UP)
        end = end > height + step ? end - step : (end > height ? height : end);
    else
        end = end + step;
    view->follow = end >= view->shown_len;
    if (!view->follow && end > 0)
        view->anchor = view->logbook->entries[view->shown[end - 1]].seq;
    pnl_unlock(&view->logbook->lock);
}

static int pnl_log_filter_key(struct pnl_log_view *view, int key)
{
    if (key == PNL_KEY_ENTER || key == PNL_KEY_ESCAPE) {
        view->editing = 0;
        view->footer = PNL_LOG_FOOTER;
        if (key == PNL_KEY_ESCAPE) {
            view->filter[0] = '\0';
            pnl_edit_init(&view->edit, view->filter, sizeof(view->filter));
        }
        return 1;
    }
    pnl_edit_key(&view->edit, key);
    return 1;
}

static int pnl_log_key(void *ctx, pnl_app_t *app, int key)
{
    struct pnl_log_view *view = ctx;

    (void)app;
    if (view->editing && !pnl_is_mouse_key(key))
        return pnl_log_filter_key(view, key);
    if (key == PNL_KEY_WHEEL_UP || key == PNL_KEY_WHEEL_DOWN)
        key = key == PNL_KEY_WHEEL_UP ? PNL_KEY_UP : PNL_KEY_DOWN;
    if (key == '/') {
        view->editing = 1;
        view->footer = PNL_LOG_FILTER_FOOTER;
    } else if (key == 'v')
        view->level = (view->level + 1) % (PNL_LEVEL_ERROR + 1);
    else if (key == PNL_KEY_END || key == 'f' || key == 'G')
        view->follow = 1;
    else if (key == PNL_KEY_UP || key == PNL_KEY_DOWN || key == 'j' || key == 'k' || key == PNL_KEY_PAGE_UP ||
             key == PNL_KEY_PAGE_DOWN || key == PNL_KEY_HOME)
        pnl_log_scroll(view, view->page, key);
    else
        return 0;
    return 1;
}

static void pnl_log_drop(pnl_app_t *app, void *ctx)
{
    struct pnl_log_view *view = ctx;

    pnl_mem_free(&app->mem, view->shown);
    pnl_mem_free(&app->mem, view);
}

static void pnl_log_cli(pnl_log_t *logbook)
{
    pnl_app_t *app = logbook->app;
    unsigned long long seq;
    char clock[16];

    pnl_lock(&logbook->lock);
    if (app->cli_json)
        putchar('[');
    for (seq = pnl_log_first(logbook); seq < logbook->next_seq; seq++) {
        const struct pnl_log_entry *entry = &logbook->entries[seq % logbook->capacity];

        pnl_log_clock(clock, sizeof(clock), entry->when);
        if (!app->cli_json) {
            printf("%s  %-5s  %s\n", clock, pnl_level_names[entry->level], pnl_log_text(logbook, entry));
            continue;
        }
        printf("%s{\"time\":\"%s\",\"level\":\"%s\",\"text\":", seq > pnl_log_first(logbook) ? "," : "", clock,
               pnl_level_names[entry->level]);
        pnl_json_string(pnl_log_text(logbook, entry));
        putchar('}');
    }
    if (app->cli_json)
        puts("]");
    pnl_unlock(&logbook->lock);
    fflush(stdout);
}

int pnl_log_open(pnl_app_t *app, pnl_log_t *logbook, const char *title)
{
    struct pnl_log_view *view;
    struct pnl_view screen;
    int status;

    if (app == NULL || logbook == NULL || title == NULL || logbook->app != app || !(app->running || app->cli))
        return PNL_ERR_ARG;
    if (app->cli) {
        pnl_log_cli(logbook);
        return PNL_OK;
    }
    view = pnl_mem_alloc(&app->mem, sizeof(*view));
    if (view == NULL)
        return PNL_ERR_NOMEM;
    memset(view, 0, sizeof(*view));
    view->logbook = logbook;
    view->follow = 1;
    view->footer = PNL_LOG_FOOTER;
    pnl_edit_init(&view->edit, view->filter, sizeof(view->filter));
    view->shown = pnl_mem_alloc(&app->mem, logbook->capacity * sizeof(*view->shown));
    memset(&screen, 0, sizeof(screen));
    screen.footer_ref = &view->footer;
    screen.typing_ref = &view->editing;
    screen.draw_fn = pnl_log_draw;
    screen.key_fn = pnl_log_key;
    screen.drop_fn = pnl_log_drop;
    screen.ctx = view;
    screen.title = pnl_mem_copy(&app->mem, title);
    status = view->shown == NULL || screen.title == NULL ? PNL_ERR_NOMEM : pnl_view_push_internal(app, &screen);
    if (status != PNL_OK) {
        pnl_mem_free(&app->mem, screen.title);
        pnl_log_drop(app, view);
    }
    return status;
}
