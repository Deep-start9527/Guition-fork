#pragma once

#include <stdbool.h>

/* Width and height describe the unrotated physical display. */
static inline void demo_rotation_to_physical(int x, int y, int width, int height,
                                            int angle, int *px, int *py)
{
    switch (angle) {
    case 90:
        *px = width - 1 - y;
        *py = x;
        break;
    case 180:
        *px = width - 1 - x;
        *py = height - 1 - y;
        break;
    case 270:
        *px = y;
        *py = height - 1 - x;
        break;
    default:
        *px = x;
        *py = y;
        break;
    }
}

static inline void demo_rotation_to_logical(int x, int y, int width, int height,
                                           int angle, int *lx, int *ly)
{
    switch (angle) {
    case 90:
        *lx = y;
        *ly = width - 1 - x;
        break;
    case 180:
        *lx = width - 1 - x;
        *ly = height - 1 - y;
        break;
    case 270:
        *lx = height - 1 - y;
        *ly = x;
        break;
    default:
        *lx = x;
        *ly = y;
        break;
    }
}

static inline void demo_rotation_panel_flags(bool base_swap, bool base_mx, bool base_my,
                                            int angle, bool *swap, bool *mx, bool *my)
{
    bool turn_swap = angle == 90 || angle == 270;
    bool turn_mx = angle == 90 || angle == 180;
    bool turn_my = angle == 180 || angle == 270;

    /* A base axis swap exchanges the rotation's mirror axes during composition. */
    if (swap) {
        *swap = base_swap != turn_swap;
    }
    if (mx) {
        *mx = base_mx != (base_swap ? turn_my : turn_mx);
    }
    if (my) {
        *my = base_my != (base_swap ? turn_mx : turn_my);
    }
}
