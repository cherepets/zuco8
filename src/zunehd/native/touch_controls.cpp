#include "touch_controls.h"
#include "png_texture.h"

#include "../../app.h"
#include "../../auxiliary.h"
#include "../../core.h"

bool toggle_menu(SDL_Renderer* renderer);
bool cart_browser_select_next(SDL_Renderer* renderer);
bool cart_browser_select_prev(SDL_Renderer* renderer);

enum
{
    DPAD_X = 0,
    DPAD_Y = 311,
    MENU_X = 88,
    MENU_Y = 242,
    O_X = 130,
    O_Y = 384,
    X_X = 176,
    X_Y = 290,
    DPAD_CELL_SIZE = 19,
    DPAD_DRAW_SIZE = 129,
    DPAD_OUTER_EDGE = 34,
    DPAD_OUTER_FAR = DPAD_DRAW_SIZE - DPAD_OUTER_EDGE,
    DPAD_MID_EDGE = 48,
    DPAD_MID_FAR = DPAD_DRAW_SIZE - DPAD_MID_EDGE,
    BUTTON_CELL_SIZE = 14,
    BUTTON_DRAW_SIZE = 96,
    MAX_TOUCHES = 4,
    GAME_X = 8,
    GAME_Y = 8,
    GAME_SIZE = 256,
    FRAME_THICKNESS = 2,

    ART_SOURCE_X = 16,
    ART_SOURCE_Y = 24,
    ART_SOURCE_SIZE = 128
};

static SDL_Texture* s_buttons_texture;
static SDL_Texture* s_dpad_texture;
static SDL_Texture* s_white_texture;
static bool s_o_was_pressed;
static bool s_x_was_pressed;
static bool s_dpad_left_was_pressed;
static bool s_dpad_right_was_pressed;
static bool s_in_menu = true;

static void DrawSprite(SDL_Renderer* renderer, SDL_Texture* texture,
    int source_x, int source_y, int source_width, int source_height,
    int destination_x, int destination_y, int destination_width,
    int destination_height)
{
    SDL_FRect source = { (float)source_x, (float)source_y,
        (float)source_width, (float)source_height };
    SDL_FRect destination = { (float)destination_x, (float)destination_y,
        (float)destination_width, (float)destination_height };

    SDL_RenderTexture(renderer, texture, &source, &destination);
}

static unsigned int ButtonMaskAt(float x, float y)
{
    unsigned int mask = 0;
    float dpad_x = x - DPAD_X;
    float dpad_y = y - DPAD_Y;

    if (dpad_x >= 0.0f && dpad_x < DPAD_DRAW_SIZE && dpad_y >= 0.0f &&
        dpad_y < DPAD_DRAW_SIZE)
    {
        bool y_outer = dpad_y < DPAD_OUTER_EDGE || dpad_y >= DPAD_OUTER_FAR;
        bool y_center = dpad_y >= DPAD_MID_EDGE && dpad_y < DPAD_MID_FAR;
        bool x_outer = dpad_x < DPAD_OUTER_EDGE || dpad_x >= DPAD_OUTER_FAR;
        bool x_center = dpad_x >= DPAD_MID_EDGE && dpad_x < DPAD_MID_FAR;
        bool horizontal = false;
        bool vertical = false;
        bool diagonal = false;

        if (y_outer)
        {
            if (x_outer) diagonal = true;
            else vertical = true;
        }
        else if (!y_center)
        {
            if (x_center) vertical = true;
            else horizontal = true;
        }
        else
        {
            horizontal = true;
        }

        if (horizontal || diagonal)
        {
            mask |= (dpad_x < DPAD_DRAW_SIZE / 2.0f) ? 1u : 2u;
        }
        if (vertical || diagonal)
        {
            mask |= (dpad_y < DPAD_DRAW_SIZE / 2.0f) ? 4u : 8u;
        }
    }
    if (x >= O_X && x < O_X + BUTTON_DRAW_SIZE && y >= O_Y &&
        y < O_Y + BUTTON_DRAW_SIZE) mask |= 16;
    if (x >= X_X && x < X_X + BUTTON_DRAW_SIZE && y >= X_Y &&
        y < X_Y + BUTTON_DRAW_SIZE) mask |= 32;
    return mask;
}

static bool PointInMenuButton(float x, float y)
{
    return x >= MENU_X && x < MENU_X + BUTTON_DRAW_SIZE && y >= MENU_Y &&
        y < MENU_Y + BUTTON_DRAW_SIZE;
}

typedef struct
{
    Uint64 id;
    bool active;
    bool down_inside;
    float last_x;
    float last_y;
} MenuTouch;

static MenuTouch s_menu_touches[MAX_TOUCHES];

// TODO: May be I overcomplicated things and it's enough to just have a bool like s_touch_started_at_menu
static bool UpdateMenuTouch(SDL_Finger** fingers, int num_fingers,
    int drawable_w, int drawable_h, bool* out_pressed)
{
    bool seen[MAX_TOUCHES] = { false };
    bool clicked = false;
    bool pressed = false;
    int index;
    int slot;

    for (index = 0; index < num_fingers && index < MAX_TOUCHES; ++index)
    {
        float x, y;

        if (!fingers[index])
        {
            continue;
        }
        x = fingers[index]->x * drawable_w;
        y = fingers[index]->y * drawable_h;

        slot = -1;
        for (int s = 0; s < MAX_TOUCHES; ++s)
        {
            if (s_menu_touches[s].active && s_menu_touches[s].id ==
                fingers[index]->id)
            {
                slot = s;
                break;
            }
        }
        if (slot == -1)
        {
            for (int s = 0; s < MAX_TOUCHES; ++s)
            {
                if (!s_menu_touches[s].active)
                {
                    slot = s;
                    break;
                }
            }
            if (slot != -1)
            {
                s_menu_touches[slot].active = true;
                s_menu_touches[slot].id = fingers[index]->id;
                s_menu_touches[slot].down_inside = PointInMenuButton(x, y);
            }
        }
        if (slot != -1)
        {
            seen[slot] = true;
            s_menu_touches[slot].last_x = x;
            s_menu_touches[slot].last_y = y;
            if (s_menu_touches[slot].down_inside && PointInMenuButton(x, y))
            {
                pressed = true;
            }
        }
    }

    for (int s = 0; s < MAX_TOUCHES; ++s)
    {
        if (s_menu_touches[s].active && !seen[s])
        {
            if (s_menu_touches[s].down_inside && PointInMenuButton(
                s_menu_touches[s].last_x, s_menu_touches[s].last_y))
            {
                clicked = true;
            }
            s_menu_touches[s].active = false;
        }
    }

    *out_pressed = pressed;
    return clicked;
}

static SDL_Texture* CreateWhitePixelTexture(SDL_Renderer* renderer)
{
    unsigned char white_pixel[4] = { 0xff, 0xff, 0xff, 0xff };
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC, 1, 1);

    if (!texture || !SDL_UpdateTexture(texture, 0, white_pixel, 4))
    {
        SDL_DestroyTexture(texture);
        return 0;
    }
    return texture;
}

bool touch_controls_initialize(SDL_Renderer* renderer)
{
    screen_rect.x = (float)GAME_X;
    screen_rect.y = (float)GAME_Y;
    screen_rect.w = (float)GAME_SIZE;
    screen_rect.h = (float)GAME_SIZE;

    s_white_texture = CreateWhitePixelTexture(renderer);
    if (!LoadPngTexture(renderer, "buttons.png", BUTTON_CELL_SIZE * 2,
        BUTTON_CELL_SIZE * 3, &s_buttons_texture) ||
        !LoadPngTexture(renderer, "dpad.png", DPAD_CELL_SIZE * 5,
        DPAD_CELL_SIZE, &s_dpad_texture))
    {
        SDL_DestroyTexture(s_buttons_texture);
        s_buttons_texture = 0;
        SDL_DestroyTexture(s_dpad_texture);
        s_dpad_texture = 0;
        return false;
    }
    return true;
}

void touch_controls_begin_frame(SDL_Renderer* renderer)
{
    int quarter_turns = 0;

    if (!renderer || !renderer->initialized)
    {
        return;
    }
    switch (SDL_ZuneGetOrientation())
    {
        case SDL_ZUNE_ORIENTATION_LANDSCAPE:
            quarter_turns = 1;
            break;
        case SDL_ZUNE_ORIENTATION_LANDSCAPE_FLIPPED:
            quarter_turns = -1;
            break;
        default:
            break;
    }
    renderer_gles2_set_rotation(renderer, quarter_turns,
        GAME_X + GAME_SIZE * 0.5f, GAME_Y + GAME_SIZE * 0.5f);
}

static void UpdateMenuInteractions(SDL_Renderer* renderer, unsigned int mask,
    bool menu_clicked)
{
    bool left_pressed = (mask & 1) != 0;
    bool right_pressed = (mask & 2) != 0;
    bool o_pressed = (mask & 16) != 0;
    bool x_pressed = (mask & 32) != 0;

    if (s_in_menu)
    {
        if (left_pressed && !s_dpad_left_was_pressed)
        {
            cart_browser_select_prev(renderer);
        }
        else if (right_pressed && !s_dpad_right_was_pressed)
        {
            cart_browser_select_next(renderer);
        }

        if (menu_clicked || (o_pressed && !s_o_was_pressed) ||
            (x_pressed && !s_x_was_pressed))
        {
            s_in_menu = !toggle_menu(renderer);
        }
    }
    else if (menu_clicked)
    {
        toggle_menu(renderer);
        s_in_menu = true;
    }

    s_dpad_left_was_pressed = left_pressed;
    s_dpad_right_was_pressed = right_pressed;
    s_o_was_pressed = o_pressed;
    s_x_was_pressed = x_pressed;
}

static void DrawCartCoverArt(SDL_Renderer* renderer)
{
    if (!s_in_menu || get_cart()->is_corrupt)
    {
        return;
    }
    DrawSprite(renderer, get_cart()->image, ART_SOURCE_X, ART_SOURCE_Y,
        ART_SOURCE_SIZE, ART_SOURCE_SIZE, GAME_X, GAME_Y, GAME_SIZE,
        GAME_SIZE);
}

void touch_controls_render(SDL_Renderer* renderer)
{
    int drawable_w = 0;
    int drawable_h = 0;
    int num_fingers = 0;
    SDL_Finger** fingers;
    unsigned int mask = 0;
    int index;
    int menu_finger_count = 0;
    bool menu_pressed = false;
    bool menu_clicked;

    if (!s_buttons_texture || !s_dpad_texture)
    {
        return;
    }

    SDL_GetRenderOutputSize(renderer, &drawable_w, &drawable_h);
    fingers = SDL_GetTouchFingers(0, &num_fingers);
    if (fingers && drawable_w > 0 && drawable_h > 0)
    {
        for (index = 0; index < num_fingers && index < MAX_TOUCHES; ++index)
        {
            if (!fingers[index])
            {
                continue;
            }
            mask |= ButtonMaskAt(fingers[index]->x * drawable_w,
                fingers[index]->y * drawable_h);
        }
        menu_finger_count = num_fingers;
    }
    touch_button_state = (uint8_t)(mask & 0x3F);

    menu_clicked = UpdateMenuTouch(fingers, menu_finger_count, drawable_w,
        drawable_h, &menu_pressed);

    UpdateMenuInteractions(renderer, mask, menu_clicked);

    DrawCartCoverArt(renderer);

    if (s_white_texture)
    {
        DrawSprite(renderer, s_white_texture, 0, 0, 1, 1,
            GAME_X - FRAME_THICKNESS, GAME_Y - FRAME_THICKNESS,
            GAME_SIZE + FRAME_THICKNESS * 2, FRAME_THICKNESS);
        DrawSprite(renderer, s_white_texture, 0, 0, 1, 1,
            GAME_X - FRAME_THICKNESS, GAME_Y + GAME_SIZE,
            GAME_SIZE + FRAME_THICKNESS * 2, FRAME_THICKNESS);
        DrawSprite(renderer, s_white_texture, 0, 0, 1, 1,
            GAME_X - FRAME_THICKNESS, GAME_Y, FRAME_THICKNESS, GAME_SIZE);
        DrawSprite(renderer, s_white_texture, 0, 0, 1, 1,
            GAME_X + GAME_SIZE, GAME_Y, FRAME_THICKNESS, GAME_SIZE);
    }

    renderer_gles2_set_rotation(renderer, 0, 0.0f, 0.0f);

    DrawSprite(renderer, s_dpad_texture, 0, 0, DPAD_CELL_SIZE,
        DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE, DPAD_DRAW_SIZE);
    if (mask & 4)
    {
        DrawSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE,
            DPAD_DRAW_SIZE);
    }
    if (mask & 2)
    {
        DrawSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE * 2, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE,
            DPAD_DRAW_SIZE);
    }
    if (mask & 8)
    {
        DrawSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE * 3, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE,
            DPAD_DRAW_SIZE);
    }
    if (mask & 1)
    {
        DrawSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE * 4, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, DPAD_X, DPAD_Y, DPAD_DRAW_SIZE,
            DPAD_DRAW_SIZE);
    }
    DrawSprite(renderer, s_buttons_texture,
        menu_pressed ? BUTTON_CELL_SIZE : 0, BUTTON_CELL_SIZE * 2,
        BUTTON_CELL_SIZE, BUTTON_CELL_SIZE, MENU_X, MENU_Y,
        BUTTON_DRAW_SIZE, BUTTON_DRAW_SIZE);
    DrawSprite(renderer, s_buttons_texture,
        (mask & 16) ? BUTTON_CELL_SIZE : 0, 0, BUTTON_CELL_SIZE,
        BUTTON_CELL_SIZE, O_X, O_Y, BUTTON_DRAW_SIZE, BUTTON_DRAW_SIZE);
    DrawSprite(renderer, s_buttons_texture,
        (mask & 32) ? BUTTON_CELL_SIZE : 0, BUTTON_CELL_SIZE,
        BUTTON_CELL_SIZE, BUTTON_CELL_SIZE, X_X, X_Y, BUTTON_DRAW_SIZE,
        BUTTON_DRAW_SIZE);
    SDL_RenderPresent(renderer);
}

void touch_controls_shutdown(void)
{
    SDL_DestroyTexture(s_dpad_texture);
    s_dpad_texture = 0;
    SDL_DestroyTexture(s_buttons_texture);
    s_buttons_texture = 0;
    SDL_DestroyTexture(s_white_texture);
    s_white_texture = 0;
}
