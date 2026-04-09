#include "pnl_internal.h"

#include <stdio.h>
#include <string.h>

#define PNL_NODE_PANEL -1
#define PNL_LAYOUT_MIN_COLUMNS 16
#define PNL_LAYOUT_MIN_ROWS 3
#define PNL_LAYOUT_STACK_ROWS 5
#define PNL_LAYOUT_FOOTER_MAX 256
#define PNL_FOCUS_MARK "\xe2\x96\x8e"

struct pnl_node {
    int kind;
    int parent;
    char *title;
    char *hints;
    pnl_draw_fn draw_fn;
    pnl_key_fn key_fn;
    void *ctx;
    int size;
    int min_size;
    int active;
};

struct pnl_rect {
    int x;
    int y;
    int width;
    int height;
};

struct pnl_hit {
    int node;
    int tabs;
    int x;
    int y;
    int width;
    int height;
    int origin_x;
    int origin_y;
};

struct pnl_layout {
    pnl_app_t *app;
    struct pnl_node *nodes;
    size_t nodes_len;
    size_t nodes_cap;
    int *visible;
    size_t visible_len;
    size_t visible_cap;
    struct pnl_hit *hits;
    size_t hits_len;
    size_t hits_cap;
    int focus;
    int has_tabs;
    char footer_text[PNL_LAYOUT_FOOTER_MAX];
    const char *footer;
};

static int pnl_layout_valid(const pnl_layout_t *layout, int node)
{
    return layout != NULL && node >= 0 && (size_t)node < layout->nodes_len;
}

static int pnl_layout_add(pnl_layout_t *layout, int parent, int kind, const char *title, int *out_node)
{
    struct pnl_node *node;
    int status;

    status = pnl_mem_grow(&layout->app->mem, (void **)&layout->nodes, &layout->nodes_cap, layout->nodes_len + 1,
                          sizeof(*node));
    if (status != PNL_OK)
        return status;
    node = &layout->nodes[layout->nodes_len];
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    node->parent = parent;
    node->active = -1;
    if (title != NULL && (node->title = pnl_mem_copy(&layout->app->mem, title)) == NULL)
        return PNL_ERR_NOMEM;
    if (out_node != NULL)
        *out_node = (int)layout->nodes_len;
    layout->nodes_len++;
    return PNL_OK;
}

static int pnl_layout_is_split(int kind)
{
    return kind == PNL_SPLIT_ROWS || kind == PNL_SPLIT_COLUMNS || kind == PNL_SPLIT_TABS;
}

int pnl_layout_create(pnl_app_t *app, int kind, pnl_layout_t **out_layout)
{
    pnl_layout_t *layout;
    int status;

    if (out_layout == NULL)
        return PNL_ERR_ARG;
    *out_layout = NULL;
    if (app == NULL || !pnl_layout_is_split(kind))
        return PNL_ERR_ARG;
    layout = pnl_mem_alloc(&app->mem, sizeof(*layout));
    if (layout == NULL)
        return PNL_ERR_NOMEM;
    memset(layout, 0, sizeof(*layout));
    layout->app = app;
    layout->focus = -1;
    status = pnl_layout_add(layout, -1, kind, NULL, NULL);
    if (status != PNL_OK) {
        pnl_layout_destroy(layout);
        return status;
    }
    *out_layout = layout;
    return PNL_OK;
}

void pnl_layout_destroy(pnl_layout_t *layout)
{
    struct pnl_mem mem;
    size_t i;

    if (layout == NULL)
        return;
    mem = layout->app->mem;
    for (i = 0; i < layout->nodes_len; i++) {
        pnl_mem_free(&mem, layout->nodes[i].title);
        pnl_mem_free(&mem, layout->nodes[i].hints);
    }
    pnl_mem_free(&mem, layout->nodes);
    pnl_mem_free(&mem, layout->visible);
    pnl_mem_free(&mem, layout->hits);
    pnl_mem_free(&mem, layout);
}

int pnl_layout_split(pnl_layout_t *layout, int parent, int kind, const char *title, int *out_node)
{
    if (!pnl_layout_valid(layout, parent) || layout->nodes[parent].kind == PNL_NODE_PANEL ||
        !pnl_layout_is_split(kind))
        return PNL_ERR_ARG;
    return pnl_layout_add(layout, parent, kind, title, out_node);
}

int pnl_layout_panel(pnl_layout_t *layout, int parent, const char *title, pnl_draw_fn draw_fn, pnl_key_fn key_fn,
                     void *panel_ctx, int *out_node)
{
    int node;
    int status;

    if (!pnl_layout_valid(layout, parent) || layout->nodes[parent].kind == PNL_NODE_PANEL || draw_fn == NULL)
        return PNL_ERR_ARG;
    status = pnl_layout_add(layout, parent, PNL_NODE_PANEL, title, &node);
    if (status != PNL_OK)
        return status;
    layout->nodes[node].draw_fn = draw_fn;
    layout->nodes[node].key_fn = key_fn;
    layout->nodes[node].ctx = panel_ctx;
    if (layout->focus < 0)
        layout->focus = node;
    if (out_node != NULL)
        *out_node = node;
    return PNL_OK;
}

int pnl_layout_size(pnl_layout_t *layout, int node, int size, int min_size)
{
    if (!pnl_layout_valid(layout, node) || node == PNL_LAYOUT_ROOT || size < 0 || min_size < 0)
        return PNL_ERR_ARG;
    layout->nodes[node].size = size;
    layout->nodes[node].min_size = min_size;
    return PNL_OK;
}

int pnl_layout_hints(pnl_layout_t *layout, int node, const char *hints)
{
    char *copy;

    if (!pnl_layout_valid(layout, node) || layout->nodes[node].kind != PNL_NODE_PANEL || hints == NULL)
        return PNL_ERR_ARG;
    copy = pnl_mem_copy(&layout->app->mem, hints);
    if (copy == NULL)
        return PNL_ERR_NOMEM;
    pnl_mem_free(&layout->app->mem, layout->nodes[node].hints);
    layout->nodes[node].hints = copy;
    return PNL_OK;
}

int pnl_layout_focus(pnl_layout_t *layout, int node)
{
    if (!pnl_layout_valid(layout, node) || layout->nodes[node].kind != PNL_NODE_PANEL)
        return PNL_ERR_ARG;
    layout->focus = node;
    return PNL_OK;
}

static int pnl_layout_next_child(const pnl_layout_t *layout, int parent, int after)
{
    size_t i;

    for (i = (size_t)(after + 1); i < layout->nodes_len; i++) {
        if (layout->nodes[i].parent == parent)
            return (int)i;
    }
    return -1;
}

static int pnl_layout_min(const pnl_layout_t *layout, int node, int axis)
{
    const struct pnl_node *n = &layout->nodes[node];

    if (n->min_size > 0)
        return n->min_size;
    if (n->size > 0)
        return n->size;
    return axis == PNL_SPLIT_COLUMNS ? PNL_LAYOUT_MIN_COLUMNS : PNL_LAYOUT_MIN_ROWS;
}

static void pnl_layout_hit_add(pnl_layout_t *layout, const pnl_canvas_t *canvas, int node, int tabs,
                               struct pnl_rect rect, int inner_y)
{
    struct pnl_hit *hit;

    if (pnl_mem_grow(&layout->app->mem, (void **)&layout->hits, &layout->hits_cap, layout->hits_len + 1,
                     sizeof(*layout->hits)) != PNL_OK)
        return;
    hit = &layout->hits[layout->hits_len++];
    hit->node = node;
    hit->tabs = tabs;
    hit->x = canvas->x + rect.x;
    hit->y = canvas->y + rect.y;
    hit->width = rect.width;
    hit->height = rect.height;
    hit->origin_x = hit->x;
    hit->origin_y = canvas->y + inner_y;
}

static int pnl_layout_inside(const pnl_layout_t *layout, int node, int ancestor)
{
    while (node >= 0 && node != ancestor)
        node = layout->nodes[node].parent;
    return node == ancestor;
}

static int pnl_layout_first_panel(const pnl_layout_t *layout, int ancestor)
{
    size_t i;

    for (i = 0; i < layout->nodes_len; i++) {
        if (layout->nodes[i].kind == PNL_NODE_PANEL && pnl_layout_inside(layout, (int)i, ancestor))
            return (int)i;
    }
    return -1;
}

static int pnl_layout_active(pnl_layout_t *layout, int tabs)
{
    struct pnl_node *n = &layout->nodes[tabs];

    if (n->active < 0 || layout->nodes[n->active].parent != tabs)
        n->active = pnl_layout_next_child(layout, tabs, -1);
    return n->active;
}

static void pnl_layout_draw_node(pnl_layout_t *layout, pnl_canvas_t *canvas, int node, struct pnl_rect rect);

static void pnl_layout_draw_panel(pnl_layout_t *layout, pnl_canvas_t *canvas, int node, struct pnl_rect rect)
{
    const struct pnl_node *n = &layout->nodes[node];
    struct pnl_canvas inner;
    struct pnl_rect full = rect;
    int focused = node == layout->focus;

    if (pnl_mem_grow(&layout->app->mem, (void **)&layout->visible, &layout->visible_cap, layout->visible_len + 1,
                     sizeof(*layout->visible)) == PNL_OK)
        layout->visible[layout->visible_len++] = node;
    if (n->title != NULL && rect.height > 0 && layout->nodes[n->parent].kind != PNL_SPLIT_TABS) {
        if (focused)
            pnl_canvas_clip(canvas, rect.x, rect.y, rect.width, PNL_FG(PNL_ROLE_ACCENT), PNL_FOCUS_MARK);
        pnl_canvas_clip(canvas, rect.x + 1, rect.y, rect.width - 1,
                        PNL_BOLD | (focused ? PNL_FG(PNL_ROLE_ACCENT) : PNL_FG(PNL_ROLE_MUTED)), n->title);
        rect.y++;
        rect.height--;
    }
    pnl_layout_hit_add(layout, canvas, node, -1, full, rect.y);
    inner.screen = canvas->screen;
    inner.focused = focused && canvas->focused;
    inner.x = canvas->x + rect.x;
    inner.y = canvas->y + rect.y;
    inner.width = rect.width;
    inner.height = rect.height;
    if (inner.width > 0 && inner.height > 0)
        n->draw_fn(n->ctx, layout->app, &inner);
}

static void pnl_layout_draw_tabs(pnl_layout_t *layout, pnl_canvas_t *canvas, int node, struct pnl_rect rect)
{
    int active = pnl_layout_active(layout, node);
    int child;
    int x = rect.x + 1;

    if (active >= 0 && pnl_layout_inside(layout, layout->focus, active))
        pnl_canvas_clip(canvas, rect.x, rect.y, rect.width, PNL_FG(PNL_ROLE_ACCENT), PNL_FOCUS_MARK);

    for (child = pnl_layout_next_child(layout, node, -1); child >= 0; child = pnl_layout_next_child(layout, node, child)) {
        const char *title = layout->nodes[child].title ? layout->nodes[child].title : "-";
        int style = child == active ? PNL_BOLD | PNL_UNDERLINE | PNL_FG(PNL_ROLE_ACCENT) : PNL_FG(PNL_ROLE_MUTED);
        struct pnl_rect label = rect;

        label.x = x;
        label.height = 1;
        x = pnl_canvas_clip(canvas, x, rect.y, rect.x + rect.width - x, style, title);
        label.width = x - label.x;
        pnl_layout_hit_add(layout, canvas, child, node, label, rect.y);
        x = pnl_canvas_clip(canvas, x, rect.y, rect.x + rect.width - x, 0, "  ");
    }
    if (active < 0 || rect.height < 2)
        return;
    rect.y += 2;
    rect.height -= 2;
    pnl_layout_draw_node(layout, canvas, active, rect);
}

static int pnl_layout_shown(const pnl_layout_t *layout, int node, int axis, int length, int gap)
{
    int used = 0;
    int shown = 0;
    int child;

    for (child = pnl_layout_next_child(layout, node, -1); child >= 0; child = pnl_layout_next_child(layout, node, child)) {
        used += pnl_layout_min(layout, child, axis) + (shown > 0 ? gap : 0);
        if (used > length && shown > 0)
            break;
        shown++;
    }
    return shown;
}

static int pnl_layout_children(const pnl_layout_t *layout, int node)
{
    int count = 0;
    int child;

    for (child = pnl_layout_next_child(layout, node, -1); child >= 0; child = pnl_layout_next_child(layout, node, child))
        count++;
    return count;
}

static int pnl_layout_stacks(const pnl_layout_t *layout, int node, struct pnl_rect rect)
{
    int count = pnl_layout_children(layout, node);

    return layout->nodes[node].kind == PNL_SPLIT_COLUMNS &&
           pnl_layout_shown(layout, node, PNL_SPLIT_COLUMNS, rect.width, 1) < count &&
           rect.height >= count * PNL_LAYOUT_STACK_ROWS;
}

static void pnl_layout_draw_split(pnl_layout_t *layout, pnl_canvas_t *canvas, int node, struct pnl_rect rect)
{
    int stacked = pnl_layout_stacks(layout, node, rect);
    int axis = stacked ? PNL_SPLIT_ROWS : layout->nodes[node].kind;
    int length = axis == PNL_SPLIT_COLUMNS ? rect.width : rect.height;
    int gap = axis == PNL_SPLIT_COLUMNS ? 1 : 0;
    int shown = stacked ? pnl_layout_children(layout, node) : pnl_layout_shown(layout, node, axis, length, gap);
    int flexible = 0;
    int left = length - gap * (shown > 0 ? shown - 1 : 0);
    int child = -1;
    int pos = 0;
    int i;

    for (i = 0, child = pnl_layout_next_child(layout, node, -1); i < shown; i++, child = pnl_layout_next_child(layout, node, child)) {
        if (layout->nodes[child].size > 0 && !stacked)
            left -= layout->nodes[child].size;
        else
            flexible++;
    }
    for (i = 0, child = pnl_layout_next_child(layout, node, -1); i < shown; i++, child = pnl_layout_next_child(layout, node, child)) {
        int size = stacked ? 0 : layout->nodes[child].size;
        struct pnl_rect part = rect;

        if (size == 0) {
            size = left > 0 ? left / flexible : 0;
            left -= size;
            flexible--;
        }
        if (size > length - pos)
            size = length - pos;
        if (axis == PNL_SPLIT_COLUMNS) {
            part.x = rect.x + pos;
            part.width = size;
            for (size = 0; i > 0 && size < rect.height; size++)
                pnl_canvas_text(canvas, part.x - 1, rect.y + size, PNL_FG(PNL_ROLE_MUTED), "\xe2\x94\x82");
            pos += part.width + gap;
        } else {
            part.y = rect.y + pos;
            part.height = size;
            pos += part.height + gap;
        }
        pnl_layout_draw_node(layout, canvas, child, part);
    }
}

static void pnl_layout_draw_node(pnl_layout_t *layout, pnl_canvas_t *canvas, int node, struct pnl_rect rect)
{
    int kind = layout->nodes[node].kind;

    if (rect.width <= 0 || rect.height <= 0)
        return;
    if (kind == PNL_NODE_PANEL)
        pnl_layout_draw_panel(layout, canvas, node, rect);
    else if (kind == PNL_SPLIT_TABS)
        pnl_layout_draw_tabs(layout, canvas, node, rect);
    else
        pnl_layout_draw_split(layout, canvas, node, rect);
}

static int pnl_layout_is_visible(const pnl_layout_t *layout, int node)
{
    size_t i;

    for (i = 0; i < layout->visible_len; i++) {
        if (layout->visible[i] == node)
            return 1;
    }
    return 0;
}

static void pnl_layout_footer(pnl_layout_t *layout)
{
    const char *hints = layout->focus >= 0 ? layout->nodes[layout->focus].hints : NULL;

    snprintf(layout->footer_text, sizeof(layout->footer_text), "%s%s%s%sesc: back", hints ? hints : "",
             hints ? "  " : "", layout->visible_len > 1 ? "tab: next panel  " : "",
             layout->has_tabs ? "[ ]: switch tab  " : "");
    layout->footer = layout->footer_text;
}

static void pnl_layout_draw(void *ctx, pnl_app_t *app, pnl_canvas_t *canvas)
{
    pnl_layout_t *layout = ctx;
    struct pnl_rect rect;

    rect.x = 0;
    rect.y = 0;
    rect.width = pnl_canvas_width(canvas);
    rect.height = pnl_canvas_height(canvas);
    layout->visible_len = 0;
    layout->hits_len = 0;
    pnl_layout_draw_node(layout, canvas, PNL_LAYOUT_ROOT, rect);
    if (layout->visible_len > 0 && !pnl_layout_is_visible(layout, layout->focus)) {
        layout->focus = layout->visible[0];
        app->dirty = 1;
    }
    pnl_layout_footer(layout);
}

static void pnl_layout_cycle(pnl_layout_t *layout, int step)
{
    size_t i;

    for (i = 0; i < layout->visible_len; i++) {
        if (layout->visible[i] == layout->focus)
            break;
    }
    if (layout->visible_len == 0)
        return;
    if (i == layout->visible_len)
        i = 0;
    else
        i = (i + (step > 0 ? 1 : layout->visible_len - 1)) % layout->visible_len;
    layout->focus = layout->visible[i];
}

static int pnl_layout_tabs_of(const pnl_layout_t *layout)
{
    int node = layout->focus;
    size_t i;

    while (node >= 0 && layout->nodes[node].kind != PNL_SPLIT_TABS)
        node = layout->nodes[node].parent;
    for (i = 0; node < 0 && i < layout->nodes_len; i++) {
        if (layout->nodes[i].kind == PNL_SPLIT_TABS)
            node = (int)i;
    }
    return node;
}

static int pnl_layout_switch(pnl_layout_t *layout, int step)
{
    int tabs = pnl_layout_tabs_of(layout);
    int active;
    int child;
    int previous = -1;
    int last = -1;

    if (tabs < 0)
        return 0;
    active = pnl_layout_active(layout, tabs);
    for (child = pnl_layout_next_child(layout, tabs, -1); child >= 0; child = pnl_layout_next_child(layout, tabs, child)) {
        if (child == active)
            previous = last;
        last = child;
    }
    child = step > 0 ? pnl_layout_next_child(layout, tabs, active) : previous;
    if (child < 0)
        child = step > 0 ? pnl_layout_next_child(layout, tabs, -1) : last;
    layout->nodes[tabs].active = child;
    if (pnl_layout_first_panel(layout, child) >= 0)
        layout->focus = pnl_layout_first_panel(layout, child);
    return 1;
}

static const struct pnl_hit *pnl_layout_hit(const pnl_layout_t *layout, int x, int y)
{
    size_t i;

    for (i = 0; i < layout->hits_len; i++) {
        const struct pnl_hit *hit = &layout->hits[i];

        if (x >= hit->x && y >= hit->y && x < hit->x + hit->width && y < hit->y + hit->height)
            return hit;
    }
    return NULL;
}

static int pnl_layout_mouse(pnl_layout_t *layout, pnl_app_t *app, int key)
{
    const struct pnl_hit *hit = pnl_layout_hit(layout, app->term.mouse_x, app->term.mouse_y);
    const struct pnl_node *n;

    if (hit == NULL)
        return 1;
    if (hit->tabs >= 0) {
        if (key == PNL_KEY_CLICK) {
            layout->nodes[hit->tabs].active = hit->node;
            if (pnl_layout_first_panel(layout, hit->node) >= 0)
                layout->focus = pnl_layout_first_panel(layout, hit->node);
        }
        return 1;
    }
    n = &layout->nodes[hit->node];
    if (key == PNL_KEY_CLICK)
        layout->focus = hit->node;
    app->mouse_ox = hit->origin_x;
    app->mouse_oy = hit->origin_y;
    if (n->key_fn != NULL && !n->key_fn(n->ctx, app, key))
        pnl_wheel_keys(app, n->key_fn, n->ctx, key);
    return 1;
}

static int pnl_layout_key(void *ctx, pnl_app_t *app, int key)
{
    pnl_layout_t *layout = ctx;
    const struct pnl_node *focused;

    if (pnl_is_mouse_key(key))
        return pnl_layout_mouse(layout, app, key);

    if (key == PNL_KEY_TAB || key == PNL_KEY_BACKTAB) {
        pnl_layout_cycle(layout, key == PNL_KEY_TAB ? 1 : -1);
        return 1;
    }
    if (key == '[' || key == ']')
        return pnl_layout_switch(layout, key == ']' ? 1 : -1);
    if (layout->focus < 0)
        return 0;
    focused = &layout->nodes[layout->focus];
    return focused->key_fn != NULL && focused->key_fn(focused->ctx, app, key);
}

static void pnl_layout_drop(pnl_app_t *app, void *ctx)
{
    (void)app;
    pnl_layout_destroy(ctx);
}

static pnl_layout_t *pnl_layout_copy(const pnl_layout_t *layout)
{
    pnl_layout_t *copy = pnl_mem_alloc(&layout->app->mem, sizeof(*copy));
    size_t i;

    if (copy == NULL)
        return NULL;
    memset(copy, 0, sizeof(*copy));
    copy->app = layout->app;
    copy->focus = layout->focus;
    if (pnl_mem_grow(&copy->app->mem, (void **)&copy->nodes, &copy->nodes_cap, layout->nodes_len,
                     sizeof(*copy->nodes)) != PNL_OK) {
        pnl_layout_destroy(copy);
        return NULL;
    }
    for (i = 0; i < layout->nodes_len; i++) {
        copy->nodes[i] = layout->nodes[i];
        copy->nodes[i].title = NULL;
        copy->nodes[i].hints = NULL;
        copy->nodes_len++;
        copy->has_tabs |= layout->nodes[i].kind == PNL_SPLIT_TABS;
        if (layout->nodes[i].hints != NULL &&
            (copy->nodes[i].hints = pnl_mem_copy(&copy->app->mem, layout->nodes[i].hints)) == NULL) {
            pnl_layout_destroy(copy);
            return NULL;
        }
        if (layout->nodes[i].title != NULL &&
            (copy->nodes[i].title = pnl_mem_copy(&copy->app->mem, layout->nodes[i].title)) == NULL) {
            pnl_layout_destroy(copy);
            return NULL;
        }
    }
    return copy;
}

int pnl_view_push_layout(pnl_app_t *app, const char *title, const pnl_layout_t *layout)
{
    pnl_layout_t *copy;
    struct pnl_view view;
    int status;

    if (app == NULL || layout == NULL || layout->app != app)
        return PNL_ERR_ARG;
    memset(&view, 0, sizeof(view));
    view.draw_fn = pnl_layout_draw;
    view.key_fn = pnl_layout_key;
    view.drop_fn = pnl_layout_drop;
    if (title != NULL && (view.title = pnl_mem_copy(&app->mem, title)) == NULL)
        return PNL_ERR_NOMEM;
    copy = pnl_layout_copy(layout);
    if (copy != NULL) {
        pnl_layout_footer(copy);
        view.footer_ref = &copy->footer;
    }
    view.ctx = copy;
    status = copy == NULL ? PNL_ERR_NOMEM : pnl_view_push_internal(app, &view);
    if (status != PNL_OK) {
        pnl_mem_free(&app->mem, view.title);
        pnl_layout_destroy(view.ctx);
    }
    return status;
}
