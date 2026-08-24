#include "touch_controls.h"
#include "png_texture.h"

#include "../../app.h"
#include "../../auxiliary.h"
#include "../../core.h"

bool toggle_menu(SDL_Renderer* renderer);
bool cart_browser_select_next(SDL_Renderer* renderer);
bool cart_browser_select_prev(SDL_Renderer* renderer);

static const int DPAD_CELL_SIZE = 19;
static const int BUTTON_CELL_SIZE = 14;
static const int MAX_TOUCHES = 4;
static const int GAME_SIZE = 256;
static const int FRAME_THICKNESS = 2;
static const int ART_SOURCE_X = 16;
static const int ART_SOURCE_Y = 24;
static const int ART_SOURCE_SIZE = 128;

typedef struct
{
    int dpad_x;
    int dpad_y;
    int dpad_draw_size;
    int dpad_outer_edge;
    int dpad_mid_edge;

    // Buttons
    int o_x;
    int o_y;
    int x_x;
    int x_y;
    int button_draw_size;
} ControlLayout;

typedef struct
{
    // Game
    int game_x;
    int game_y;

    ControlLayout player_0;
    ControlLayout player_1;
    bool has_player_1_controls;
    int dpad_touch_padding;
    int button_touch_padding;
} UiLayout;

static const UiLayout s_portrait_layout =
{
    // Game
    8, 8,
    { 0, 298, 133, 35, 49, 133, 382, 174, 277, 98 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    false,
    0,
    0
};

static const UiLayout s_landscape_layout =
{
    // Game
    8, 112,
    { 172, 378, 95, 25, 35, 88, 378, 4, 403, 70 },
    { 172, 7, 95, 25, 35, 88, 32, 4, 7, 70 },
    true,
    7,
    0
};

static const UiLayout s_landscape_flipped_layout =
{
    // Game
    8, 112,
    { 24, 7, 95, 25, 35, 12, 378, 96, 403, 70 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    false,
    7,
    7
};

static SDL_Texture* s_buttons_texture;
static SDL_Texture* s_dpad_texture;
static SDL_Texture* s_white_texture;
static bool s_o_was_pressed;
static bool s_x_was_pressed;
static bool s_dpad_left_was_pressed;
static bool s_dpad_right_was_pressed;
static bool s_in_menu = true;
static const UiLayout* s_layout = &s_portrait_layout;
static int s_controls_quarter_turns;

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

static void DrawControlSprite(SDL_Renderer* renderer, SDL_Texture* texture,
    int source_x, int source_y, int source_width, int source_height,
    int destination_x, int destination_y, int destination_width,
    int destination_height)
{
    if (s_controls_quarter_turns != 0)
    {
        renderer_gles2_set_rotation(renderer, s_controls_quarter_turns,
            destination_x + destination_width * 0.5f,
            destination_y + destination_height * 0.5f);
    }
    DrawSprite(renderer, texture, source_x, source_y, source_width,
        source_height, destination_x, destination_y, destination_width,
        destination_height);
}

static unsigned int ButtonMaskAt(const ControlLayout* controls, float x, float y)
{
    unsigned int mask = 0;
    float dpad_center_x = controls->dpad_x + controls->dpad_draw_size * 0.5f;
    float dpad_center_y = controls->dpad_y + controls->dpad_draw_size * 0.5f;
    float dpad_touch_x = x;
    float dpad_touch_y = y;
    float dpad_x;
    float dpad_y;
    bool y_outer;
    bool y_center;
    bool x_outer;
    bool x_center;
    bool horizontal;
    bool vertical;
    bool diagonal;
    int dpad_outer_far = controls->dpad_draw_size - controls->dpad_outer_edge;
    int dpad_mid_far = controls->dpad_draw_size - controls->dpad_mid_edge;
    float dpad_last_coordinate = controls->dpad_draw_size - 0.001f;

    if (s_controls_quarter_turns == 1)
    {
        float offset_x = dpad_touch_x - dpad_center_x;
        float offset_y = dpad_touch_y - dpad_center_y;
        dpad_touch_x = dpad_center_x - offset_y;
        dpad_touch_y = dpad_center_y + offset_x;
    }
    else if (s_controls_quarter_turns == -1)
    {
        float offset_x = dpad_touch_x - dpad_center_x;
        float offset_y = dpad_touch_y - dpad_center_y;
        dpad_touch_x = dpad_center_x + offset_y;
        dpad_touch_y = dpad_center_y - offset_x;
    }
    dpad_x = dpad_touch_x - controls->dpad_x;
    dpad_y = dpad_touch_y - controls->dpad_y;

    if (dpad_x >= -s_layout->dpad_touch_padding &&
        dpad_x < controls->dpad_draw_size + s_layout->dpad_touch_padding &&
        dpad_y >= -s_layout->dpad_touch_padding &&
        dpad_y < controls->dpad_draw_size + s_layout->dpad_touch_padding)
    {
        if (dpad_x < 0.0f) dpad_x = 0.0f;
        else if (dpad_x >= controls->dpad_draw_size)
            dpad_x = dpad_last_coordinate;
        if (dpad_y < 0.0f) dpad_y = 0.0f;
        else if (dpad_y >= controls->dpad_draw_size)
            dpad_y = dpad_last_coordinate;
        y_outer = dpad_y < controls->dpad_outer_edge ||
            dpad_y >= dpad_outer_far;
        y_center = dpad_y >= controls->dpad_mid_edge &&
            dpad_y < dpad_mid_far;
        x_outer = dpad_x < controls->dpad_outer_edge ||
            dpad_x >= dpad_outer_far;
        x_center = dpad_x >= controls->dpad_mid_edge &&
            dpad_x < dpad_mid_far;
        horizontal = false;
        vertical = false;
        diagonal = false;

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
            mask |= (dpad_x < controls->dpad_draw_size / 2.0f) ? 1u : 2u;
        }
        if (vertical || diagonal)
        {
            mask |= (dpad_y < controls->dpad_draw_size / 2.0f) ? 4u : 8u;
        }
    }
    if (x >= controls->o_x - s_layout->button_touch_padding &&
        x < controls->o_x + controls->button_draw_size + s_layout->button_touch_padding &&
        y >= controls->o_y - s_layout->button_touch_padding &&
        y < controls->o_y + controls->button_draw_size + s_layout->button_touch_padding)
        mask |= 16;
    if (x >= controls->x_x - s_layout->button_touch_padding &&
        x < controls->x_x + controls->button_draw_size + s_layout->button_touch_padding &&
        y >= controls->x_y - s_layout->button_touch_padding &&
        y < controls->x_y + controls->button_draw_size + s_layout->button_touch_padding)
        mask |= 32;
    return mask;
}

static bool PointInGameScreen(float x, float y)
{
    return x >= s_layout->game_x && x < s_layout->game_x + GAME_SIZE &&
        y >= s_layout->game_y && y < s_layout->game_y + GAME_SIZE;
}

typedef struct
{
    Uint64 id;
    bool active;
    bool down_inside;
    float last_x;
    float last_y;
} GameScreenTouch;

static GameScreenTouch s_game_screen_touches[MAX_TOUCHES];

static bool UpdateGameScreenTouch(SDL_Finger** fingers, int num_fingers,
    int drawable_w, int drawable_h)
{
    bool seen[MAX_TOUCHES] = { false };
    bool clicked = false;
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
            if (s_game_screen_touches[s].active && s_game_screen_touches[s].id ==
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
                if (!s_game_screen_touches[s].active)
                {
                    slot = s;
                    break;
                }
            }
            if (slot != -1)
            {
                s_game_screen_touches[slot].active = true;
                s_game_screen_touches[slot].id = fingers[index]->id;
                s_game_screen_touches[slot].down_inside = PointInGameScreen(x, y);
            }
        }
        if (slot != -1)
        {
            seen[slot] = true;
            s_game_screen_touches[slot].last_x = x;
            s_game_screen_touches[slot].last_y = y;
        }
    }

    for (int s = 0; s < MAX_TOUCHES; ++s)
    {
        if (s_game_screen_touches[s].active && !seen[s])
        {
            if (s_game_screen_touches[s].down_inside && PointInGameScreen(
                s_game_screen_touches[s].last_x, s_game_screen_touches[s].last_y))
            {
                clicked = true;
            }
            s_game_screen_touches[s].active = false;
        }
    }

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

static void ApplyLayout(const UiLayout* layout)
{
    s_layout = layout;
    screen_rect.x = (float)s_layout->game_x;
    screen_rect.y = (float)s_layout->game_y;
    screen_rect.w = (float)GAME_SIZE;
    screen_rect.h = (float)GAME_SIZE;
}

bool touch_controls_initialize(SDL_Renderer* renderer)
{
    ApplyLayout(&s_portrait_layout);

    s_white_texture = CreateWhitePixelTexture(renderer);
    if (!LoadPngTexture(renderer, "buttons.png", BUTTON_CELL_SIZE * 2,
        BUTTON_CELL_SIZE * 2, &s_buttons_texture) ||
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
    const UiLayout* layout = &s_portrait_layout;
    int quarter_turns = 0;

    if (!renderer || !renderer->initialized)
    {
        return;
    }
    switch (SDL_ZuneGetOrientation())
    {
        case SDL_ZUNE_ORIENTATION_LANDSCAPE:
            layout = &s_landscape_layout;
            quarter_turns = 1;
            break;
        case SDL_ZUNE_ORIENTATION_LANDSCAPE_FLIPPED:
            layout = &s_landscape_flipped_layout;
            quarter_turns = -1;
            break;
        default:
            break;
    }
    ApplyLayout(layout);
    s_controls_quarter_turns = quarter_turns;
    renderer_gles2_set_rotation(renderer, quarter_turns,
        s_layout->game_x + GAME_SIZE * 0.5f,
        s_layout->game_y + GAME_SIZE * 0.5f);
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
            if (!get_cart()->is_corrupt)
            {
                toggle_menu(renderer);
                s_in_menu = false;
            }
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
        ART_SOURCE_SIZE, ART_SOURCE_SIZE, s_layout->game_x, s_layout->game_y,
        GAME_SIZE, GAME_SIZE);
}

static void DrawControls(SDL_Renderer* renderer, const ControlLayout* controls,
    unsigned int mask)
{
    DrawControlSprite(renderer, s_dpad_texture, 0, 0, DPAD_CELL_SIZE,
        DPAD_CELL_SIZE, controls->dpad_x, controls->dpad_y,
        controls->dpad_draw_size, controls->dpad_draw_size);
    if (mask & 4)
    {
        DrawControlSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, controls->dpad_x,
            controls->dpad_y, controls->dpad_draw_size,
            controls->dpad_draw_size);
    }
    if (mask & 2)
    {
        DrawControlSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE * 2, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, controls->dpad_x,
            controls->dpad_y, controls->dpad_draw_size,
            controls->dpad_draw_size);
    }
    if (mask & 8)
    {
        DrawControlSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE * 3, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, controls->dpad_x,
            controls->dpad_y, controls->dpad_draw_size,
            controls->dpad_draw_size);
    }
    if (mask & 1)
    {
        DrawControlSprite(renderer, s_dpad_texture, DPAD_CELL_SIZE * 4, 0,
            DPAD_CELL_SIZE, DPAD_CELL_SIZE, controls->dpad_x,
            controls->dpad_y, controls->dpad_draw_size,
            controls->dpad_draw_size);
    }
    DrawControlSprite(renderer, s_buttons_texture,
        (mask & 16) ? BUTTON_CELL_SIZE : 0, 0, BUTTON_CELL_SIZE,
        BUTTON_CELL_SIZE, controls->o_x, controls->o_y,
        controls->button_draw_size, controls->button_draw_size);
    DrawControlSprite(renderer, s_buttons_texture,
        (mask & 32) ? BUTTON_CELL_SIZE : 0, BUTTON_CELL_SIZE,
        BUTTON_CELL_SIZE, BUTTON_CELL_SIZE, controls->x_x, controls->x_y,
        controls->button_draw_size, controls->button_draw_size);
}

void touch_controls_render(SDL_Renderer* renderer)
{
    int drawable_w = 0;
    int drawable_h = 0;
    int num_fingers = 0;
    SDL_Finger** fingers;
    unsigned int player_0_mask = 0;
    unsigned int player_1_mask = 0;
    int index;
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
            float x = fingers[index]->x * drawable_w;
            float y = fingers[index]->y * drawable_h;
            unsigned int mask = ButtonMaskAt(&s_layout->player_0, x, y);

            if (mask != 0)
            {
                player_0_mask |= mask;
            }
            else if (s_layout->has_player_1_controls)
            {
                player_1_mask |= ButtonMaskAt(&s_layout->player_1, x, y);
            }
        }
    }
    touch_button_state_player_0 = (uint8_t)(player_0_mask & 0x3F);
    touch_button_state_player_1 = s_in_menu ? 0 :
        (uint8_t)(player_1_mask & 0x3F);

    menu_clicked = UpdateGameScreenTouch(fingers, num_fingers, drawable_w,
        drawable_h);

    UpdateMenuInteractions(renderer, player_0_mask | player_1_mask,
        menu_clicked);

    DrawCartCoverArt(renderer);

    if (s_white_texture)
    {
        DrawSprite(renderer, s_white_texture, 0, 0, 1, 1,
            s_layout->game_x - FRAME_THICKNESS,
            s_layout->game_y - FRAME_THICKNESS,
            GAME_SIZE + FRAME_THICKNESS * 2, FRAME_THICKNESS);
        DrawSprite(renderer, s_white_texture, 0, 0, 1, 1,
            s_layout->game_x - FRAME_THICKNESS,
            s_layout->game_y + GAME_SIZE,
            GAME_SIZE + FRAME_THICKNESS * 2, FRAME_THICKNESS);
        DrawSprite(renderer, s_white_texture, 0, 0, 1, 1,
            s_layout->game_x - FRAME_THICKNESS, s_layout->game_y,
            FRAME_THICKNESS, GAME_SIZE);
        DrawSprite(renderer, s_white_texture, 0, 0, 1, 1,
            s_layout->game_x + GAME_SIZE, s_layout->game_y,
            FRAME_THICKNESS, GAME_SIZE);
    }

    renderer_gles2_set_rotation(renderer, 0, 0.0f, 0.0f);

    DrawControls(renderer, &s_layout->player_0, player_0_mask);
    if (s_layout->has_player_1_controls)
    {
        DrawControls(renderer, &s_layout->player_1, player_1_mask);
    }
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
