#include <windows.h>
#include <intrin.h>
#include <string>
#include <stdlib.h>
#include <xmmintrin.h>

using namespace std;

/* Todo list:
    - Optimize
    - Save with the correct number
UI:
    - Better rendering in the window
*/

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "gif.h"

#define UpDown_(var) ((void *) &(var))
#define start_sliders() int slider_count = 0;
#define new_slider()    ++slider_count

#define get_time(t)  QueryPerformanceCounter((t));
#define time_elapsed(t0, t1, CPM) ((t1).QuadPart - (t0).QuadPart) / (CPM);

#define MAX(a, b) (((a) > (b)) ? (a) : (b));
#define MIN(a, b) (((a) < (b)) ? (a) : (b));
#define clamp(a, b, c) (((a) > (c)) ? (c) : (((a) < (b)) ? (b) : (a)))

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;
typedef LARGE_INTEGER      ms;

typedef char      int8;
typedef short     int16;
typedef int       int32;
typedef long long int64;

#define Bytes_Per_Pixel 4
struct bitmap
{
    uint8 *Memory = NULL;
    int32 Width   = 0;
    int32 Height  = 0;

    uint8 *Original  = NULL;
    BITMAPINFO *Info = NULL;
};

const int file_name_size = 400;
struct Texture {
    bitmap texture = {0};
    
    int elements;
    int element_side;

    char name[file_name_size] = "";
};

struct RECT_f {
    float left, top, right, bottom;
};

RECT rect_from_rectf(RECT_f rect) {
    RECT result;
    result.left   = rect.left;
    result.top    = rect.top;
    result.right  = rect.right;
    result.bottom = rect.bottom;

    return result;
}

struct v2 {
    int x, y;
};

bitmap     Main_Buffer = {0};
BITMAPINFO Main_Info   = {0};

HWND Window = {0};

bool left_button_down  = false;
bool right_button_down = false;

int mousewheel_counter = 0;
v2  mouse_position     = {0, 0};

bool handled_press_left  = false;
bool handled_press_right = false;
bool changed_size = false;

#define Total_Caps 121
bitmap caps_data[Total_Caps];

#define MAX_TEXTURES 20
Texture textures[MAX_TEXTURES];
int total_textures = 0;

#define FIRST_CHAR_SAVED 32
#define LAST_CHAR_SAVED  126
#define CHAR_SAVED_RANGE LAST_CHAR_SAVED - FIRST_CHAR_SAVED + 1

#define N_SIZES 3
struct Font_Data {
    float scale[N_SIZES];
    
    int ascent;
    int descent;
    
    int advance;
    int line_height;
    int line_gap;
    
    stbtt_fontinfo info;

    stbtt_pack_context spc[N_SIZES];
    stbtt_packedchar   chardata[N_SIZES][CHAR_SAVED_RANGE];
};

enum font_types {
    Small_Font  = 0,
    Medium_Font = 1,
    Big_Font    = 2,
};

Font_Data Font;

struct Color {
    int R, G, B, A;
};

struct Color_hsv {
    float h, s, v;
    uint8 alpha;
};
inline max_c(uint8 a, uint8 b, uint8 c) {
    uint8 temp = MAX(a, b);
    return MAX(temp, c);
}
inline min_c(uint8 a, uint8 b, uint8 c) {
    uint8 temp = MIN(a, b);
    return MIN(temp, c);
}
Color_hsv rgb_to_hsv(Color c) {
    uint8 min = min_c(c.R, c.G, c.B);
    uint8 max = max_c(c.R, c.G, c.B);
    uint8 delta = max - min;

    Color_hsv result;
    result.alpha = c.A;
    result.v     = (float) max / 255.0;

    if (max > 0) result.s = (float) delta / (float) max;
    else {
        result.s = 0;
        result.h = 0;
        return result;
    }

    if      (c.R == max) result.h = (float) (c.G - c.B) / (float) delta;
    else if (c.G == max) result.h = (float) (c.B - c.R) / (float) delta + 2;
    else                 result.h = (float) (c.R - c.G) / (float) delta + 4;

    result.h *= 60;
    if (result.h < 0) result.h += 360;

    return result;
}

Color hsv_to_rgb(Color_hsv c) {

    uint8 v = c.v * 255;
    if (c.s == 0) return {v, v, v, c.alpha};

    c.h /= 60; 
    int sector = floor(c.h);
    float diff = c.h - sector;
    uint8 p = v * (1.0 - c.s);
    uint8 q = v * (1.0 - c.s * diff);
    uint8 t = v * (1.0 - c.s * (1.0 - diff));
    
    switch (sector) {
        case 0: return {v, t, p, c.alpha};
        case 1: return {q, v, p, c.alpha};
        case 2: return {p, v, t, c.alpha};
        case 3: return {p, q, v, c.alpha};
        case 4: return {t, p, v, c.alpha};
        case 5: return {v, p, q, c.alpha};
    }
    return {0, 0, 0, c.alpha};
}

Color shift_hue(Color c, int shift) {
    Color_hsv hsv = rgb_to_hsv(c);
    
    hsv.h += shift;
    while(hsv.h >= 360) hsv.h -= 360;
    while(hsv.h <  0)   hsv.h += 360;

    return hsv_to_rgb(hsv);
}

const Color WHITE      = {255, 255, 255, 255};
const Color DARK_WHITE = {180, 180, 180, 255};
const Color LIGHT_GRAY = { 95,  95,  95, 255};
const Color GRAY       = { 50,  50,  50, 255};
const Color BLACK      = {  0,   0,   0, 255};
const Color GREEN      = { 70, 200,  80, 255};
const Color DARK_GREEN = { 50, 125,  60, 255};
const Color BLUE       = { 70, 200, 255, 255};
const Color DARK_BLUE  = { 20, 120, 170, 255};
const Color RED        = {255,   0,   0, 255};
const Color ERROR_RED  = {201,  36,  24, 255};
const Color WARNING    = {247, 194,  32, 255};

struct Color_Palette {
    Color button_color, highlight_button_color;
    Color value_color,  highlight_value_color;
    Color text_color,   highlight_text_color;
    Color background_color;
};

//                               btn         h_btn  val    h_val  txt        h_txt  bg
Color_Palette default_palette = {DARK_WHITE, WHITE, BLUE,  BLUE,  DARK_BLUE, BLUE,  BLACK};
Color_Palette slider_palette  = {DARK_WHITE, WHITE, BLACK, BLACK, DARK_BLUE, BLUE,  BLACK};
Color_Palette header_palette  = {LIGHT_GRAY, WHITE, BLACK, BLACK, WHITE,     BLACK, GRAY};
Color_Palette save_palette    = {DARK_WHITE, WHITE, BLACK, BLACK, BLACK,     BLACK, BLUE};
Color_Palette no_save_palette = {DARK_WHITE, WHITE, BLACK, BLACK, DARK_BLUE, BLUE,  BLACK};

const int Packed_Font_W = 500;
const int Packed_Font_H = 500;

RECT get_rect(int x, int y, int width, int height) {
    return {x, y, x + width, y + height};
}

void init_font(int sizes[]) {
    char ttf_buffer[1<<20];
    int temp;

    fread(ttf_buffer, 1, 1<<20, fopen("Font/consola.ttf", "rb"));

    stbtt_InitFont(&Font.info, (const uint8 *) ttf_buffer, stbtt_GetFontOffsetForIndex((const uint8 *) ttf_buffer, 0));
    stbtt_GetFontVMetrics(&Font.info, &Font.ascent, &Font.descent, &Font.line_gap);
    stbtt_GetCodepointHMetrics(&Font.info, 'A', &Font.advance, &temp);
    Font.line_height = (Font.ascent - Font.descent + Font.line_gap);

    uint8 temp_bitmap[N_SIZES][Packed_Font_W][Packed_Font_H];

    for(int i = 0; i < N_SIZES; i++) {
        Font.scale[i] = (float) sizes[i] / (float) (Font.ascent - Font.descent);
        stbtt_PackBegin(&Font.spc[i], &temp_bitmap[i][0][0], Packed_Font_W, Packed_Font_H, 0, 1, NULL);
        stbtt_PackFontRange(&Font.spc[i], (const uint8 *) ttf_buffer, 0, sizes[i], FIRST_CHAR_SAVED, CHAR_SAVED_RANGE, Font.chardata[i]);    
        stbtt_PackEnd(&Font.spc[i]);
    }
}

void render_text(char *text, int length, int font_type, RECT dest_rect, Color c = {255, 255, 255, 255}) {
    float xpos = (dest_rect.left + dest_rect.right) / 2 - (length * Font.advance * Font.scale[font_type]) / 2;
    float ypos = (dest_rect.top + dest_rect.bottom) / 2 + Font.ascent * Font.scale[font_type] / 2;
    stbtt_aligned_quad quad;

    int x, y;
    
    for (uint8 ch = 0; ch < length; ch++) {

        stbtt_GetPackedQuad(&Font.chardata[font_type][0], 1, 1, text[ch] - FIRST_CHAR_SAVED, &xpos, &ypos, &quad, true);
        
        uint32 *Dest   = (uint32 *) Main_Buffer.Memory + (uint32) (Main_Buffer.Width * quad.y0 + quad.x0);
        uint8  *Source = Font.spc[font_type].pixels + (uint32) (Packed_Font_W * quad.t0 + quad.s0);

        int max_x = MIN(xpos + quad.x1 - quad.x0, Main_Buffer.Width);
        int max_y = MIN(ypos + quad.y1 - quad.y0, Main_Buffer.Height);

        int skipped_horizontal_pixels = (xpos + quad.x1 - quad.x0) - max_x;
        for (y = ypos; y < max_y; y++) {
            for (x = xpos; x < max_x; x++) {
                if (xpos > 0 && ypos > 0) {
                    float SA = *Source / 255.0;
                    uint8 SR = c.R * SA;
                    uint8 SG = c.G * SA;
                    uint8 SB = c.B * SA;

                    float DA = ((*Dest >> 24) & 0xff) / 255.0;
                    uint8 DR =  (*Dest >> 16) & 0xff;
                    uint8 DG =  (*Dest >>  8) & 0xff;
                    uint8 DB =  (*Dest >>  0) & 0xff;

                    uint8 A = 255 * (SA + DA - SA*DA);
                    uint8 R = DR * (1 - SA) + SR;
                    uint8 G = DG * (1 - SA) + SG;
                    uint8 B = DB * (1 - SA) + SB;

                    *Dest = (A << 24) | (R << 16) | (G << 8) | B;
                }
                Source++;
                Dest++;
            }
            Source += (uint32) (Packed_Font_W     - (quad.s1 - quad.s0) + skipped_horizontal_pixels);
            Dest   += (uint32) (Main_Buffer.Width - (max_x - xpos));
        }

        xpos = ceil(xpos);
    }
}

void render_text(int number, int font_type, RECT dest_rect, Color c = {255, 255, 255, 255}) {
    char *text = (char *)to_string(number).c_str();
    
    int length;
    for (length = 0; text[length] != '\0'; length++) {}
    
    render_text(text, length, font_type, dest_rect, c);
}

void render_text(float number, int digits, int font_type, RECT dest_rect, Color c = {255, 255, 255, 255}) {
    char *text = (char *)to_string(number).c_str();
    
    int length;
    for (length = 0; text[length] != '\0'; length++) {}
    length = length < digits + 2 ? length : digits + 2;

    render_text(text, length, font_type, dest_rect, c);
}

void initialize_main_buffer() {
    Main_Buffer.Info = &Main_Info;
    Main_Buffer.Info->bmiHeader.biSize        = sizeof(Main_Buffer.Info->bmiHeader);
    Main_Buffer.Info->bmiHeader.biPlanes      = 1;
    Main_Buffer.Info->bmiHeader.biBitCount    = 8 * Bytes_Per_Pixel;
    Main_Buffer.Info->bmiHeader.biCompression = BI_RGB;
    Main_Buffer.Memory = NULL;
}

void resize_main_buffer(int new_width, int new_height) {

    if (Main_Buffer.Memory) free(Main_Buffer.Memory);

    Main_Buffer.Info->bmiHeader.biWidth  =  new_width;
    Main_Buffer.Info->bmiHeader.biHeight = -new_height;
    Main_Buffer.Memory = (uint8 *) malloc((new_width * new_height) * Bytes_Per_Pixel);
    Main_Buffer.Width  = new_width;
    Main_Buffer.Height = new_height;
}

void blit_main_buffer_to_window() {
    HDC DeviceContext = GetDC(Window);

    StretchDIBits(DeviceContext,
                  0, 0, Main_Buffer.Width, Main_Buffer.Height, // destination
                  0, 0, Main_Buffer.Width, Main_Buffer.Height, // source
                  Main_Buffer.Memory, Main_Buffer.Info, DIB_RGB_COLORS, SRCCOPY);
}

v2 screen_to_window_position(POINT pos) {
    v2 result;

    RECT rect;
    GetClientRect( Window, (LPRECT) &rect);
    ClientToScreen(Window, (LPPOINT)&rect.left);
    ClientToScreen(Window, (LPPOINT)&rect.right);
    result.x = pos.x - rect.left;
    result.y = pos.y - rect.top;

    return result;
}

LRESULT CALLBACK main_window_callback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;
    switch(Message)
    {
        case WM_SIZE: {
            changed_size = true;

            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int width  = ClientRect.right  - ClientRect.left;
            int height = ClientRect.bottom - ClientRect.top;

            resize_main_buffer(width, height);
        } break;
        case WM_DESTROY: {
            PostQuitMessage(0);
        } break;
        case WM_CLOSE: {
            PostQuitMessage(0);
        } break;
        case WM_LBUTTONDOWN:
            left_button_down   = true;
            handled_press_left = false;
            break;
        case WM_LBUTTONUP:
            left_button_down   = false;
            handled_press_left = true;
            break;
        case WM_RBUTTONDOWN:
            right_button_down   = true;
            handled_press_right = false;
            break;
        case WM_RBUTTONUP:
            right_button_down   = false;
            handled_press_right = true;
            break;
        case WM_MOUSEWHEEL: {
            auto key_state = GET_KEYSTATE_WPARAM(WParam);
            if (key_state) break;

            auto delta = GET_WHEEL_DELTA_WPARAM(WParam);
            mousewheel_counter += delta;
        } break;
        default: {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return Result;
}

#define INITIAL_WIDTH  1120
#define INITIAL_HEIGHT 750

void start_main_window() {
    WNDCLASS WindowClass = {};
    WindowClass.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc   = main_window_callback;
    WindowClass.hInstance     = GetModuleHandle(0);
    WindowClass.lpszClassName = "GeneralWindowClass";

    RegisterClass(&WindowClass);
    Window = CreateWindowEx(
        0,
        WindowClass.lpszClassName,
        "Caps Pictures",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_VISIBLE | WS_THICKFRAME,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        INITIAL_WIDTH,
        INITIAL_HEIGHT,
        0, 0, GetModuleHandle(0), 0);
}

int handle_window_messages() {
    MSG Message;
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
        if (Message.message == WM_QUIT) return -1;
    }
    return 0;
}

void render_filled_rectangle(RECT rectangle, Color color)
{
    uint32 c = (color.A << 24) | (color.R << 16) | (color.G << 8) | color.B;
    
    int starting_x = MAX(rectangle.left,   0);
    int starting_y = MAX(rectangle.top,    0);
    int ending_x   = MIN(rectangle.right,  Main_Buffer.Width);
    int ending_y   = MIN(rectangle.bottom, Main_Buffer.Height);
    
    if (starting_x > Main_Buffer.Width)  return;
    if (starting_y > Main_Buffer.Height) return;
    if (ending_x < 0) return;
    if (ending_y < 0) return;

    int Pitch = Main_Buffer.Width * Bytes_Per_Pixel;
    uint8 *Row = Main_Buffer.Memory;
    Row += starting_x*Bytes_Per_Pixel + starting_y*Pitch;

    for (int Y = starting_y; Y < ending_y; Y++) {        
        uint32 *Pixel = (uint32 *)Row;   
        for (int X = starting_x; X < ending_x; X++) {
            *Pixel = c;
            Pixel++;
        }
        Row += Pitch;
    }
}

void render_rectangle(RECT rectangle, Color color, uint8 thickness)
{
    if (thickness * 2 >= rectangle.right - rectangle.left || thickness * 2 >= rectangle.bottom - rectangle.top) {
        render_filled_rectangle(rectangle, color);
        return;
    }

    RECT top_rect    = {rectangle.left, rectangle.top, rectangle.right, rectangle.top + thickness};
    RECT bottom_rect = {rectangle.left, rectangle.bottom - thickness, rectangle.right, rectangle.bottom};
    RECT left_rect   = {rectangle.left, rectangle.top + thickness, rectangle.left + thickness, rectangle.bottom - thickness};
    RECT right_rect  = {rectangle.right - thickness, rectangle.top + thickness, rectangle.right, rectangle.bottom - thickness};

    render_filled_rectangle(top_rect, color);
    render_filled_rectangle(bottom_rect, color);
    render_filled_rectangle(left_rect, color);
    render_filled_rectangle(right_rect, color);
}

void blit_bitmap_to_bitmap(bitmap *Dest, bitmap *Source, int x, int y, int width, int height) {

    if (width  < 0) width  = Source->Width;
    if (height < 0) height = Source->Height;

    int starting_x = MAX(x, 0);
    int starting_y = MAX(y, 0);
    int ending_x   = MIN(x + width,  Dest->Width);
    int ending_y   = MIN(y + height, Dest->Height);
    
    if (starting_x > Dest->Width)  return;
    if (starting_y > Dest->Height) return;
    if (ending_x < 0) return;
    if (ending_y < 0) return;

    int x_bitmap, y_bitmap;

    int Dest_Pitch = Dest->Width * Bytes_Per_Pixel;
    uint8 *Row     = (uint8 *)  Dest->Memory;
    uint32 *Texels = (uint32 *) Source->Memory;
    Row += starting_x * Bytes_Per_Pixel + starting_y * Dest_Pitch;

    float width_scale  = (float) Source->Width  / (float) width;
    float height_scale = (float) Source->Height / (float) height;

    for (int Y = starting_y; Y < ending_y; Y++) {
        uint32 *Pixel = (uint32 *)Row;
        y_bitmap = (Y - y) * height_scale;
        for (int X = starting_x; X < ending_x; X++) {
            x_bitmap = (X - x) * width_scale;
            uint32 source_pixel = Texels[x_bitmap + y_bitmap * Source->Width];

            float SA = ((source_pixel >> 24) & 0xff) / 255.0;
            uint8 SR =  (source_pixel >> 16) & 0xff;
            uint8 SG =  (source_pixel >>  8) & 0xff;
            uint8 SB =  (source_pixel >>  0) & 0xff;

            float DA = ((*Pixel >> 24) & 0xff) / 255.0;
            uint8 DR =  (*Pixel >> 16) & 0xff;
            uint8 DG =  (*Pixel >>  8) & 0xff;
            uint8 DB =  (*Pixel >>  0) & 0xff;

            uint8 A = 255 * (SA + DA - SA*DA);
            uint8 R = DR * (1 - SA) + SR;
            uint8 G = DG * (1 - SA) + SG;
            uint8 B = DB * (1 - SA) + SB;

            *Pixel = (A << 24) | (R << 16) | (G << 8) | B;
            Pixel++;
        }
        Row += Dest_Pitch;
    }
}

void blit_texture_to_bitmap(bitmap *Dest, int element_index, v2 center, int w, Color color, int texture_index) {

    int x = center.x - w / 2;
    int y = center.y - w / 2;

    int starting_x = MAX(x, 0);
    int starting_y = MAX(y, 0);
    int ending_x   = MIN(x + w, Dest->Width);
    int ending_y   = MIN(y + w, Dest->Height);
    
    if (starting_x > Dest->Width)  return;
    if (starting_y > Dest->Height) return;
    if (ending_x < 0) return;
    if (ending_y < 0) return;

    Texture t = textures[texture_index];

    int x_bitmap, y_bitmap;

    int Dest_Pitch = Dest->Width * Bytes_Per_Pixel;
    uint8 *Row     = (uint8 *)  Dest->Memory;
    uint32 *Texels = (uint32 *) t.texture.Memory;
    Row += starting_x * Bytes_Per_Pixel + starting_y * Dest_Pitch;

    float width_scale  = (float) t.element_side / (float) w;
    float height_scale = (float) t.element_side / (float) w;

    for (int Y = starting_y; Y < ending_y; Y++) {
        uint32 *Pixel = (uint32 *)Row;
        y_bitmap = (Y - y) * height_scale;
        for (int X = starting_x; X < ending_x; X++) {
            x_bitmap = (X - x) * width_scale;
            uint32 source_pixel = Texels[x_bitmap + y_bitmap * t.texture.Width + element_index * t.element_side];

            float SA = ((source_pixel >> 24) & 0xff) / 255.0;
            uint8 SR = color.R * SA;
            uint8 SG = color.G * SA;
            uint8 SB = color.B * SA;

            float DA = ((*Pixel >> 24) & 0xff) / 255.0;
            uint8 DR =  (*Pixel >> 16) & 0xff;
            uint8 DG =  (*Pixel >>  8) & 0xff;
            uint8 DB =  (*Pixel >>  0) & 0xff;

            uint8 A = 255 * (SA + DA - SA*DA);
            uint8 R = DR * (1 - SA) + SR;
            uint8 G = DG * (1 - SA) + SG;
            uint8 B = DB * (1 - SA) + SB;

            *Pixel = (A << 24) | (R << 16) | (G << 8) | B;
            Pixel++;
        }
        Row += Dest_Pitch;
    }
}

int get_w(RECT rect)   { return rect.right  - rect.left; }
int get_h(RECT rect)   { return rect.bottom - rect.top; }
int get_w(RECT_f rect) { return rect.right  - rect.left; }
int get_h(RECT_f rect) { return rect.bottom - rect.top; }

void render_bitmap_to_screen(bitmap *Source, RECT dest_rect, RECT source_rect) {
    int starting_x = MAX(dest_rect.left, 0);
    int starting_y = MAX(dest_rect.top,  0);
    int ending_x   = MIN(dest_rect.right,  Main_Buffer.Width);
    int ending_y   = MIN(dest_rect.bottom, Main_Buffer.Height);
    
    if (starting_x > Main_Buffer.Width)  return;
    if (starting_y > Main_Buffer.Height) return;
    if (ending_x < 0) return;
    if (ending_y < 0) return;

    int x_bitmap, y_bitmap;

    int Dest_Pitch = Main_Buffer.Width * Bytes_Per_Pixel;
    uint8 *Row     = (uint8 *)  Main_Buffer.Memory;
    uint32 *Texels = (uint32 *) Source->Memory;
    Row += starting_x * Bytes_Per_Pixel + starting_y * Dest_Pitch;

    float width_scale  = (float) get_w(source_rect) / (float) get_w(dest_rect);
    float height_scale = (float) get_h(source_rect) / (float) get_h(dest_rect);

    for (int Y = starting_y; Y < ending_y; Y++) {        
        uint32 *Pixel = (uint32 *)Row;
        
        y_bitmap = (Y - dest_rect.top) * height_scale;
        if (y_bitmap + source_rect.top  >= Source->Height || y_bitmap + source_rect.top  < 0) {
            Row += Dest_Pitch;
            continue;
        }

        for (int X = starting_x; X < ending_x; X++) {

            x_bitmap = (X - dest_rect.left) * width_scale;
            if (x_bitmap + source_rect.left < 0 || x_bitmap + source_rect.left >= Source->Width) {
                Pixel++;
                continue;
            }

            uint32 texel_position = (x_bitmap + source_rect.left) + (y_bitmap + source_rect.top) * Source->Width;
            uint32 source_pixel = Texels[texel_position];

            float SA = ((source_pixel >> 24) & 0xff) / 255.0;
            uint8 SR =  (source_pixel >> 16) & 0xff;
            uint8 SG =  (source_pixel >>  8) & 0xff;
            uint8 SB =  (source_pixel >>  0) & 0xff;

            float DA = ((*Pixel >> 24) & 0xff) / 255.0;
            uint8 DR =  (*Pixel >> 16) & 0xff;
            uint8 DG =  (*Pixel >>  8) & 0xff;
            uint8 DB =  (*Pixel >>  0) & 0xff;

            uint8 A = 255 * (SA + DA - SA*DA);
            uint8 R = DR * (1 - SA) + SR;
            uint8 G = DG * (1 - SA) + SG;
            uint8 B = DB * (1 - SA) + SB;

            *Pixel = (A << 24) | (R << 16) | (G << 8) | B;
            Pixel++;
        }
        Row += Dest_Pitch;
    }

}

#define SQRT_2 1.41421356237
#define SQRT_3 1.73205080757
void compute_centers_hex(v2 *centers, int x_caps, int y_caps, int radius) {
    int delta_vertical   = (SQRT_3 * radius) + 0.5;
    int delta_horizontal = 2 * radius;

    int current_y = radius;
    bool indent = false;

    int counter = 0;

    for (int i = 0; i < y_caps; i++) {
        int current_x = (1 + (int)indent) * radius;

        for (int j = 0; j < x_caps; j++) {
            centers[counter] = {current_x, current_y};
            current_x += delta_horizontal;
            counter++;
        }
        current_y += delta_vertical;
        indent = !indent;
    }
}

void compute_centers_randomly(v2 *centers, int centers_length, int bmp_width, int bmp_height) {

    for (int i = 0; i < centers_length; i++) {
        centers[i].x = rand() % bmp_width;
        centers[i].y = rand() % bmp_height;
    }
}

void compute_centers_grid(v2 *centers, int x_caps, int y_caps, int side) {
    for (int i = 0; i < x_caps; i++) {
        for (int j = 0; j < y_caps; j++) {
            centers[i * y_caps + j] = {side * i + side/2, side * j + side/2};
        }
    }
}

Color compute_color_average(bitmap image, int center_x, int center_y, int radius) {
    Color result = {0, 0, 0, 255};

    int starting_x = MAX(center_x - radius, 0);
    int starting_y = MAX(center_y - radius, 0);
    int ending_x   = MIN(center_x + radius, image.Width);
    int ending_y   = MIN(center_y + radius, image.Height);
    
    if (starting_x > image.Width)  return result;
    if (starting_y > image.Height) return result;
    if (ending_x < 0)              return result;
    if (ending_y < 0)              return result;

    int32 Pitch = image.Width * Bytes_Per_Pixel;
    uint8 *Row = image.Memory;
    Row += starting_x * Bytes_Per_Pixel + starting_y * Pitch;

    uint64 R = 0;
    uint64 G = 0;
    uint64 B = 0;
    float count = 0;

    for (int Y = starting_y; Y < ending_y; Y++) {        
        uint32 *Pixel = (uint32 *)Row;   
        for (int X = starting_x; X < ending_x; X++) {
            R += (*Pixel >> 16) & 0xff;
            G += (*Pixel >>  8) & 0xff;
            B += (*Pixel >>  0) & 0xff;
            Pixel++;

            count += ((*Pixel >> 24) & 0xff) / 255.0; // scale by the alpha value
        }
        Row += Pitch;
    }

    if (count == 0) return result;

    result.R = R / count;
    result.G = G / count;
    result.B = B / count;
    
    return result;
}

enum Centers_Style {
    Grid,
    Hexagonal,
    Random,
};

enum Render_Centers_By {
    Randomly,
    Brightness,
    Darkness,
    Render_Centers_By_Count,
};

enum Size_Settings {
    Not_Inverted_Size,
    Inverted_Size,
    Random_Size,
    Size_Settings_Count,
};

enum Show_Settings {
    Show_Background_Settings,
    Show_Save_Settings,
    Show_Gif_Settings,
};

#define Caps_Style    0
#define Texture_Style 1

struct General_Settings {
    int result_brightness = 50;
    int result_contrast   = 50;

    int source_brightness = 50;
    int source_contrast   = 50;

    bool save_as_png = true;
    bool save_as_bmp = false;
    bool save_as_jpg = false;

    float scale = 1.0;

    bool lock_zoom = true;

    int show_settings = Show_Save_Settings;
    int show_settings_visible = true;

    int texture_selected = 0;

    // background
    Color bg_color = BLACK;
    bool use_original_as_background = false;

    // Grid
    int caps_or_texture = Texture_Style;
    bool style_settings_visible = true;
    
    // centers_hex is selected
    int x_hex  = 10;
    int y_hex  = 10;
    int radius = 10;
    
    // centers_grid is selected
    int x_grid = 10;
    int y_grid = 10;
    int width  = 20;

    // centers_random is selected
    int total_centers = 2000;
    int random_radius = 10;
    int render_centers_by = Randomly;

    // Style
    int centers_style = Grid;
    bool centers_settings_visible = false;

    // caps
    bool inverse_caps = false;
    bool by_color     = false;
    bool hard_max     = false;
    bool compute_with_limits = false;

    // texture
    int adjusted_brightness = 50;
    int adjusted_hue        = 0;

    bool shuffle_centers = false;
    bool allow_oversizing = false;

    int range_low  = 0;
    int range_high = 1000;

    int size_style = Not_Inverted_Size;
};
General_Settings settings;
General_Settings last_saved_settings;

void shuffle(v2 *array, int array_length, int shuffle_times) {
    for (int i = 0; i < shuffle_times; i++) {
        for (int v = 0; v < array_length - 1; v++) {
            if (rand() % 2 == 0) {
                v2 temp = array[v];
                array[v] = array[v + 1];
                array[v + 1] = temp;
            }
        } 
    }
}

void sort_caps(v2 *centers, int *indexes, int length, bool reverse) {

    for (int i = 0; i < length-1; i++) {
        for (int j = 0; j < length-1-i; j++) {
            bool condition = indexes[j] > indexes[j+1];
            if (reverse) condition = indexes[j] < indexes[j+1];

            if (condition) {
                v2 temp_v = centers[j];
                centers[j]   = centers[j+1];
                centers[j+1] = temp_v;

                int temp_i = indexes[j];
                indexes[j]   = indexes[j+1];
                indexes[j+1] = temp_i;
            }
        }
    }
}

void sort_textures_by_birghtness(v2 *centers, Color *colors, float *brightness, int length, bool reverse) {

    for (int i = 0; i < length-1; i++) {
        for (int j = 0; j < length-1-i; j++) {
            bool condition = brightness[j] > brightness[j+1];
            if (reverse) condition = brightness[j] < brightness[j+1];

            if (condition) {
                v2 temp_v = centers[j];
                centers[j]   = centers[j+1];
                centers[j+1] = temp_v;

                float temp_b = brightness[j];
                brightness[j]   = brightness[j+1];
                brightness[j+1] = temp_b;

                Color temp_c = colors[j];
                colors[j]   = colors[j+1];
                colors[j+1] = temp_c;
            }
        }
    }
}

int *compute_indexes(bitmap image, v2 *centers, int number_of_centers, int radius) {

    int *indexes = (int *) malloc(sizeof(int) * number_of_centers);
    assert(indexes);

    int min_index = Total_Caps - 1;
    int max_index = 0;

    for (int i = 0; i < number_of_centers; i++) {
        Color c = compute_color_average(image, centers[i].x, centers[i].y, radius);
        auto gray_value = (c.R + c.G + c.B) / 3.0;
        gray_value = clamp(gray_value, 0, 255.0);
        indexes[i] = (gray_value / 255.0) * (Total_Caps - 1);
        
        indexes[i] = clamp(indexes[i], 0, Total_Caps - 1);

        if (indexes[i] > max_index) max_index = indexes[i];
        if (indexes[i] < min_index) min_index = indexes[i];
    }

    float scale = (float) (Total_Caps - 1) / (float) (max_index - min_index);
    for (int i = 0; i < number_of_centers; i++) {
        indexes[i] = (indexes[i] - min_index) * scale;
        indexes[i] = clamp(indexes[i], 0, Total_Caps); // should not happen (just to be sure)
    }

    return indexes;
}

int *compute_indexes_by_color(bitmap image, v2 *centers, int number_of_centers, int radius, int adjusted_brightness, int adjusted_hue) {
    
    int *indexes = (int *) malloc(sizeof(int) * number_of_centers);
    assert(indexes);

    Color caps_colors[Total_Caps];
    for (int i = 0; i < Total_Caps; i++) {
        caps_colors[i] = compute_color_average(caps_data[i], 0, 0, caps_data[i].Width);
    }

    for (int i = 0; i < number_of_centers; i++) {
        Color c = compute_color_average(image, centers[i].x, centers[i].y, radius);
        if (adjusted_hue != 0) c = shift_hue(c, adjusted_hue);

        float b = (float) settings.adjusted_brightness / 50.0;
        uint8 R = clamp((int) (c.R * b), 0, 255);
        uint8 G = clamp((int) (c.G * b), 0, 255);
        uint8 B = clamp((int) (c.B * b), 0, 255);

        float min_distance = 255.0 * 255.0 * 255.0;
        int   min_index    = -1;
        for (int j = 0; j < Total_Caps; j++) {
            float distance;
            if (settings.hard_max) {
                distance = MAX((caps_colors[j].R - R) * (caps_colors[j].R - R), (caps_colors[j].G - G) * (caps_colors[j].G - G));
                distance = MAX((caps_colors[j].B - B) * (caps_colors[j].B - B), distance);
            } else {
                distance = (caps_colors[j].R - R) * (caps_colors[j].R - R) +
                           (caps_colors[j].G - G) * (caps_colors[j].G - G) +
                           (caps_colors[j].B - B) * (caps_colors[j].B - B);
            }

            if (distance <= min_distance) {
                min_distance = distance;
                min_index    = j;
            }
        }
        assert(min_index >= 0);
        indexes[i] = min_index;
    }

    return indexes;
}

Color *compute_centers_colors(bitmap image, v2 *centers, int number_of_centers, int radius) {

    Color *colors = (Color *) malloc(sizeof(Color) * number_of_centers);
    assert(colors);

    for (int i = 0; i < number_of_centers; i++) {
        colors[i] = compute_color_average(image, centers[i].x, centers[i].y, radius);
    }

    return colors;
}

void compute_dimensions_from_radius(bitmap image) {
    if (settings.radius <= 0) settings.radius = 1;
    settings.x_hex = (float) image.Width  / (float) (2      * settings.radius) + 1;
    settings.y_hex = (float) image.Height / (float) (SQRT_2 * settings.radius) + 1;
}

void compute_dimensions_from_x_hex(bitmap image) {  
    if (settings.x_hex <= 0) settings.x_hex = 1;
    settings.radius = (float) image.Width / (float) (settings.x_hex * 2);

    if (settings.radius <= 0) settings.radius = 1;
    settings.y_hex = (float) image.Height / (float) (SQRT_2 * settings.radius) + 1;
}

void compute_dimensions_from_y_hex(bitmap image) {
    if (settings.y_hex <= 0) settings.y_hex = 1;
    settings.radius = (float) image.Width / (float) (settings.y_hex * SQRT_2);

    if (settings.radius <= 0) settings.radius = 1;
    settings.x_hex = (float) image.Height / (float) (2 * settings.radius) + 1;
}

void compute_dimensions_from_width(bitmap image) {
    if (settings.width <= 0) settings.width = 1;
    settings.x_grid = (float) image.Width  / (float) settings.width + 1;
    settings.y_grid = (float) image.Height / (float) settings.width + 1;
}

void compute_dimensions_from_x_grid(bitmap image) {
    if (settings.x_grid <= 0) settings.x_grid = 1;
    settings.width = (float) image.Width / (float) settings.x_grid;

    if (settings.width <= 0) settings.width = 1;
    settings.y_grid = (float) image.Height / (float) settings.width + 1;
}

void compute_dimensions_from_y_grid(bitmap image) {
    if (settings.y_grid <= 0) settings.y_grid = 1;
    settings.width = (float) image.Height / (float) settings.y_grid;

    if (settings.width <= 0) settings.width = 1;
    settings.x_grid = (float) image.Width / (float) settings.width + 1;
}

void crop_image(bitmap *image, RECT_f displayed_crop_rectangle) {

    RECT crop_rectangle;
    crop_rectangle.left = max((int) displayed_crop_rectangle.left, 0);
    crop_rectangle.top  = max((int) displayed_crop_rectangle.top, 0);
    crop_rectangle.right  = min((int) displayed_crop_rectangle.right, image->Width);
    crop_rectangle.bottom = min((int) displayed_crop_rectangle.bottom, image->Height);

    int new_width  = crop_rectangle.right - crop_rectangle.left;
    int new_height = crop_rectangle.bottom - crop_rectangle.top;

    uint32 *old_memory = (uint32 *) image->Memory;
    uint32 *new_memory = (uint32 *) malloc(new_width * new_height * Bytes_Per_Pixel);
    if (new_memory == NULL) return;
    
    for (int i = 0; i < new_width; i++) {
        for (int j = 0; j < new_height; j++) {
            new_memory[i + j * new_width] = old_memory[(i + crop_rectangle.left) + (j + crop_rectangle.top) * image->Width];
        }
    }

    free(image->Memory);

    image->Memory = (uint8 *) new_memory;
    image->Width  = new_width;
    image->Height = new_height;
}

void adjust_bpp(bitmap *image, int Bpp) {
    
    if (image->Memory == NULL) return;

    if (Bpp == 4) return;
    else if (Bpp == 3) {
        uint32 *new_memory = (uint32 *) malloc(image->Width * image->Height * Bytes_Per_Pixel);
        uint8  *Bytes      = (uint8 *)  image->Memory;
        for (int p = 0; p < image->Width * image->Height; p++) {
            new_memory[p] = (255 << 24) | (Bytes[p*4] << 16) | (Bytes[p*4 + 1] << 8) | (Bytes[p*4 + 2] << 0);
        }
        free(image->Memory);
        image->Memory = (uint8 *) new_memory;
        
    } else if (Bpp == 1) {
        uint32 *new_memory = (uint32 *) malloc(image->Width * image->Height * Bytes_Per_Pixel);
        uint8  *Pixels     = (uint8 *)  image->Memory;
        for (int p = 0; p < image->Width * image->Height; p++) {
            new_memory[p] = (255 << 24) | (Pixels[p*4 + 1] << 16) | (Pixels[p*4 + 1] << 8) | (Pixels[p*4 + 1] << 0);
        }
        free(image->Memory);
        image->Memory = (uint8 *) new_memory;
        
    } else {
        printf("ERROR: wrong bytes_per_pixel passed to 'adjust_bpp' function.\n");
    }
}

void swap_red_and_blue_channels(bitmap *image) {
    uint32 *Pixels = (uint32 *) image->Memory;
    for (int p = 0; p < image->Width * image->Height; p++) {
        uint8 A = (Pixels[p] >> 24) & 0xff;
        uint8 B = (Pixels[p] >> 16) & 0xff;
        uint8 G = (Pixels[p] >>  8) & 0xff;
        uint8 R = (Pixels[p] >>  0) & 0xff;
        
        Pixels[p] = (A << 24) | (R << 16) | (G << 8) | (B << 0);
    }

    if (image->Original) {
        uint32 *Pixels = (uint32 *) image->Original;
        for (int p = 0; p < image->Width * image->Height; p++) {
            uint8 A = (Pixels[p] >> 24) & 0xff;
            uint8 B = (Pixels[p] >> 16) & 0xff;
            uint8 G = (Pixels[p] >>  8) & 0xff;
            uint8 R = (Pixels[p] >>  0) & 0xff;
            
            Pixels[p] = (A << 24) | (R << 16) | (G << 8) | (B << 0);
        }
    }
}

void premultiply_alpha(bitmap *image) {
    uint32 *Pixels = (uint32 *) image->Memory;
    for (int p = 0; p < image->Width * image->Height; p++) {
        uint8 A = ((Pixels[p] >> 24) & 0xff);
        float A_scaled = (float) A / 255.0;
        uint8 R = ((Pixels[p] >> 16) & 0xff) * A_scaled;
        uint8 G = ((Pixels[p] >>  8) & 0xff) * A_scaled;
        uint8 B = ((Pixels[p] >>  0) & 0xff) * A_scaled;
        
        Pixels[p] = (A << 24) | (R << 16) | (G << 8) | (B << 0);
    }
}

void apply_brightness(bitmap image, int brightness, bool use_original_as_source = false) {
    if (brightness == 50 && !use_original_as_source) return;

    float scale = 255.0 / 49.0;
    int b = (brightness - 50) * scale;

    uint32 *dest   = (uint32 *) image.Memory;
    uint32 *source = (uint32 *) image.Memory;
    if (use_original_as_source) source = (uint32 *) image.Original;

    uint32 n_pixels = image.Width * image.Height;
    uint32 n_pixels_fast = n_pixels - (n_pixels % 4);

    uint64 t0 = __rdtsc();

    __m128i brightness_4x = _mm_set1_epi32(b);
    __m128i mask_FF = _mm_set1_epi32(0xFF);
    for (int p = 0; p < n_pixels_fast; p += 4) {

        __m128i pixel = _mm_loadu_si128((__m128i *) (source + p));

        __m128i A = _mm_and_si128(_mm_srai_epi32(pixel, 24), mask_FF);
        __m128i R = _mm_and_si128(_mm_srai_epi32(pixel, 16), mask_FF);
        __m128i G = _mm_and_si128(_mm_srai_epi32(pixel,  8), mask_FF);
        __m128i B = _mm_and_si128(               pixel,      mask_FF);

        R = _mm_add_epi32(R, brightness_4x);
        G = _mm_add_epi32(G, brightness_4x);
        B = _mm_add_epi32(B, brightness_4x);

        R = _mm_max_epi16(_mm_min_epi16(R, _mm_set1_epi32(255)), _mm_set1_epi32(0));
        G = _mm_max_epi16(_mm_min_epi16(G, _mm_set1_epi32(255)), _mm_set1_epi32(0));
        B = _mm_max_epi16(_mm_min_epi16(B, _mm_set1_epi32(255)), _mm_set1_epi32(0));

        __m128i shift_A = _mm_slli_epi32(A, 24);
        __m128i shift_R = _mm_slli_epi32(R, 16);
        __m128i shift_G = _mm_slli_epi32(G, 8);

        __m128i pixels = _mm_or_si128(_mm_or_si128(_mm_or_si128(shift_A, shift_R), shift_G), B);
        *((__m128i *) (dest + p)) = pixels;
    }

    for (int p = n_pixels_fast; p < n_pixels; p++) {
        uint8 A = (source[p] >> 24);
        uint8 R = (source[p] >> 16) & 0xff;
        uint8 G = (source[p] >>  8) & 0xff;
        uint8 B = (source[p] >>  0) & 0xff;

        if (R > 0) R = clamp((int) R + b, 0, 255);
        if (G > 0) G = clamp((int) G + b, 0, 255);
        if (B > 0) B = clamp((int) B + b, 0, 255);
        
        dest[p] = (A << 24) | (R << 16) | (G << 8) | (B << 0);
    }

    uint64 t1 = __rdtsc();
    float cycles           = (float) (t1 - t0);
    float cycles_per_pixel = cycles / (float) n_pixels;
    // printf("Brightness: Total cycles: %.0f, cycles per pixel %f\n", cycles, cycles_per_pixel);
}

void apply_contrast(bitmap image, int contrast, bool use_original_as_source = false) {
    if (contrast == 50 && !use_original_as_source) return;

    float scale = 255.0 / 49.0;
    int   c = (contrast - 50) * scale;
    float f = 259.0 / 255.0 * (255.0 + c) / (259.0 - c);

    uint32 *dest   = (uint32 *) image.Memory;
    uint32 *source = (uint32 *) image.Memory;
    if (use_original_as_source) source = (uint32 *) image.Original;

    uint32 n_pixels = image.Width * image.Height;
    uint32 n_pixels_fast = n_pixels - (n_pixels % 4);

    uint64 t0 = __rdtsc();

    __m128i mask_FF  = _mm_set1_epi32(0xFF);
    __m128i wide_128 = _mm_set1_epi32(128);
    __m128  wide_f   = _mm_set_ps1(f);
    for (int p = 0; p < n_pixels_fast; p += 4) {

        __m128i pixel = _mm_loadu_si128((__m128i *) (source + p));

        __m128i A = _mm_and_si128(_mm_srai_epi32(pixel, 24), mask_FF);
        __m128i R = _mm_and_si128(_mm_srai_epi32(pixel, 16), mask_FF);
        __m128i G = _mm_and_si128(_mm_srai_epi32(pixel,  8), mask_FF);
        __m128i B = _mm_and_si128(               pixel,      mask_FF);

        __m128 Rf = _mm_mul_ps(_mm_cvtepi32_ps(_mm_sub_epi32(R, wide_128)), wide_f);
        __m128 Gf = _mm_mul_ps(_mm_cvtepi32_ps(_mm_sub_epi32(G, wide_128)), wide_f);
        __m128 Bf = _mm_mul_ps(_mm_cvtepi32_ps(_mm_sub_epi32(B, wide_128)), wide_f);

        R = _mm_add_epi32(_mm_cvtps_epi32(Rf), wide_128);
        G = _mm_add_epi32(_mm_cvtps_epi32(Gf), wide_128);
        B = _mm_add_epi32(_mm_cvtps_epi32(Bf), wide_128);

        R = _mm_max_epi16(_mm_min_epi16(R, _mm_set1_epi32(255)), _mm_set1_epi32(0));
        G = _mm_max_epi16(_mm_min_epi16(G, _mm_set1_epi32(255)), _mm_set1_epi32(0));
        B = _mm_max_epi16(_mm_min_epi16(B, _mm_set1_epi32(255)), _mm_set1_epi32(0));

        __m128i shift_A = _mm_slli_epi32(A, 24);
        __m128i shift_R = _mm_slli_epi32(R, 16);
        __m128i shift_G = _mm_slli_epi32(G, 8);

        __m128i pixels = _mm_or_si128(_mm_or_si128(_mm_or_si128(shift_A, shift_R), shift_G), B);
        *((__m128i *) (dest + p)) = pixels;
    }

    for (int p = n_pixels_fast; p < n_pixels; p++) {
        int R_pre = (source[p] >> 16) & 0xff;
        int G_pre = (source[p] >>  8) & 0xff;
        int B_pre = (source[p] >>  0) & 0xff;

        uint8 A = ((source[p] >> 24) & 0xff);
        uint8 R = clamp(f * (R_pre - 128) + 128, 0, 255);
        uint8 G = clamp(f * (G_pre - 128) + 128, 0, 255);
        uint8 B = clamp(f * (B_pre - 128) + 128, 0, 255);
        
        dest[p] = (A << 24) | (R << 16) | (G << 8) | (B << 0);
    }

    uint64 t1 = __rdtsc();
    float cycles           = (float) (t1 - t0);
    float cycles_per_pixel = cycles / (float) n_pixels;
    // printf("Contrast:   Total cycles: %.0f, cycles per pixel %f\n", cycles, cycles_per_pixel);

}

v2 *get_centers(bitmap image, int *number_of_centers, int *radius) {
    v2 *centers;
    if (settings.centers_style == Random) {
        (*number_of_centers) = settings.total_centers;
        (*radius)            = settings.random_radius;

        centers = (v2 *) malloc(sizeof(v2) * (*number_of_centers));
        assert(centers);
        compute_centers_randomly(centers, (*number_of_centers), image.Width, image.Height);
    } else if (settings.centers_style == Grid) {
        (*number_of_centers) = settings.x_grid * settings.y_grid;
        (*radius)            = settings.width / 2.0;

        centers = (v2 *) malloc(sizeof(v2) * (*number_of_centers));
        assert(centers);
        compute_centers_grid(centers, settings.x_grid, settings.y_grid, settings.width);
    } else {
        assert(settings.centers_style == Hexagonal);
        (*number_of_centers) = settings.x_hex * settings.y_hex;
        (*radius)            = settings.radius;

        centers = (v2 *) malloc(sizeof(v2) * (*number_of_centers));
        assert(centers);
        compute_centers_hex(centers, settings.x_hex, settings.y_hex, settings.radius);
        if (settings.shuffle_centers && settings.centers_style != Random) shuffle(centers, *number_of_centers, 100);
    }
    return centers;
}

float *compute_table(bitmap image, v2 *centers, int number_of_centers, int radius, int adjusted_brightness, int adjusted_hue) {
    
    float *table = (float *) malloc(sizeof(float) * number_of_centers * Total_Caps);
    assert(table);

    // Todo: do on startup
    Color caps_colors[Total_Caps];
    for (int i = 0; i < Total_Caps; i++) {
        caps_colors[i] = compute_color_average(caps_data[i], 0, 0, caps_data[i].Width);
    }

    for (int i = 0; i < number_of_centers; i++) {
        Color c = compute_color_average(image, centers[i].x, centers[i].y, radius);
        if (adjusted_hue != 0) c = shift_hue(c, adjusted_hue);

        float b = (float) settings.adjusted_brightness / 50.0;
        uint8 R = clamp((int) (c.R * b), 0, 255);
        uint8 G = clamp((int) (c.G * b), 0, 255);
        uint8 B = clamp((int) (c.B * b), 0, 255);

        for (int j = 0; j < Total_Caps; j++) {
            Color c = caps_colors[j];
            float distance;
            if (settings.hard_max) {
                distance = MAX((c.R - R) * (c.R - R), (c.G - G) * (c.G - G));
                distance = MAX((c.B - B) * (c.B - B), distance);
                distance /= 255.0 * 255.0; // Normalize
            } else {
                distance = (c.R - R) * (c.R - R) + (c.G - G) * (c.G - G) + (c.B - B) * (c.B - B);
                distance /= 3.0 * 255.0 * 255.0; // Normalize
            }
            table[i * Total_Caps + j] = distance;
        }
    }

    return table;
}

int caps_count[Total_Caps] = {0};

int find_min(float *distance_table, int *used_caps, int *paired_centers, int number_of_centers) {
    int min_index = -1;
    for (int i = 0; i < number_of_centers; i++) {
        if (paired_centers[i] != 0) continue;
        for (int j = 0; j < Total_Caps; j++) {
            if (used_caps[j] >= caps_count[j]) continue;

            if (min_index == -1) {
                min_index = i * Total_Caps + j;
            } else if (distance_table[min_index] > distance_table[i * Total_Caps + j]) {
                min_index = i * Total_Caps + j;
            }
        }
    }

    return min_index;
}

bitmap create_image_caps_with_caps_limit(bitmap image) {
    int number_of_centers, radius;
    v2 *centers = get_centers(image, &number_of_centers, &radius);

    int total_owned_caps = 0;
    for (int i = 0; i < Total_Caps; i++) total_owned_caps += caps_count[i];
    if (number_of_centers > total_owned_caps) {
        printf("You don't have enough caps for this. Counted %d, but the picture requires %d\n", total_owned_caps, number_of_centers);
        free(centers);
        return (bitmap) {0};
    }
    
    float *distance_table = compute_table(image, centers, number_of_centers, radius, settings.adjusted_brightness, settings.adjusted_hue);

    int used_caps[Total_Caps] = {0};
    int paired_centers[number_of_centers] = {0};

    for (int i = 0; i < number_of_centers; i++) {
        int min_index = find_min(distance_table, used_caps, paired_centers, number_of_centers);

        int cap = min_index % Total_Caps;
        used_caps[cap]++;
        
        int center = min_index / Total_Caps;
        paired_centers[center] = cap + 1; // + 1 is a hack for now
    }

    for (int i = 0; i < number_of_centers; i++) {
        centers[i].x *= settings.scale;
        centers[i].y *= settings.scale;
    }
    
    bitmap result_bitmap;
    result_bitmap.Width  = image.Width  * settings.scale;
    result_bitmap.Height = image.Height * settings.scale;
    result_bitmap.Memory   = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    result_bitmap.Original = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    if (result_bitmap.Memory == NULL || result_bitmap.Original == NULL) {
        free(centers);
        free(distance_table);
        return result_bitmap;
    }

    if (!settings.use_original_as_background) {
        for (uint32 i = 0; i < result_bitmap.Width * result_bitmap.Height; i++) {
            result_bitmap.Memory[i*4 + 0] = settings.bg_color.B;
            result_bitmap.Memory[i*4 + 1] = settings.bg_color.G;
            result_bitmap.Memory[i*4 + 2] = settings.bg_color.R;
            result_bitmap.Memory[i*4 + 3] = settings.bg_color.A;
        }
    } else {
        blit_bitmap_to_bitmap(&result_bitmap, &image, 0, 0, result_bitmap.Width, result_bitmap.Height);
    }

    int dim = 2 * radius * settings.scale;
    for (int i = 0; i < number_of_centers; i++) {
        blit_bitmap_to_bitmap(&result_bitmap, &caps_data[paired_centers[i] - 1], centers[i].x - dim / 2, centers[i].y - dim / 2, dim, dim);
    }
    memcpy(result_bitmap.Original, result_bitmap.Memory, result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    
    free(centers);
    free(distance_table);

    return result_bitmap;
}

bitmap create_image_caps(bitmap image) {
    
    int number_of_centers, radius;
    v2 *centers = get_centers(image, &number_of_centers, &radius);
    
    int *indexes;
    if (settings.by_color)
        indexes = compute_indexes_by_color(image, centers, number_of_centers, radius, settings.adjusted_brightness, settings.adjusted_hue);
    else
        indexes = compute_indexes(image, centers, number_of_centers, radius);
    
    if (settings.inverse_caps) {
        for (int i = 0; i < number_of_centers; i++) indexes[i] = Total_Caps - indexes[i] - 1;
    }

    for (int i = 0; i < number_of_centers; i++) {
        centers[i].x *= settings.scale;
        centers[i].y *= settings.scale;
    }
    
    bitmap result_bitmap;
    result_bitmap.Width  = image.Width  * settings.scale;
    result_bitmap.Height = image.Height * settings.scale;
    result_bitmap.Memory   = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    result_bitmap.Original = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    if (result_bitmap.Memory == NULL || result_bitmap.Original == NULL) {
        free(centers);
        free(indexes);
        return result_bitmap;
    }

    if (!settings.use_original_as_background) {
        for (uint32 i = 0; i < result_bitmap.Width * result_bitmap.Height; i++) {
            result_bitmap.Memory[i*4 + 0] = settings.bg_color.B;
            result_bitmap.Memory[i*4 + 1] = settings.bg_color.G;
            result_bitmap.Memory[i*4 + 2] = settings.bg_color.R;
            result_bitmap.Memory[i*4 + 3] = settings.bg_color.A;
        }
    } else {
        blit_bitmap_to_bitmap(&result_bitmap, &image, 0, 0, result_bitmap.Width, result_bitmap.Height);
    }
    
    if (settings.centers_style == Random && settings.render_centers_by != Randomly) {
        sort_caps(centers, indexes, number_of_centers, settings.render_centers_by != Brightness);
    }

    int dim = 2 * radius * settings.scale;
    for (int i = 0; i < number_of_centers; i++) {
        blit_bitmap_to_bitmap(&result_bitmap, &caps_data[indexes[i]], centers[i].x - dim / 2, centers[i].y - dim / 2, dim, dim);
    }

    memcpy(result_bitmap.Original, result_bitmap.Memory, result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    apply_brightness(result_bitmap, settings.result_brightness);
    apply_contrast(result_bitmap,   settings.result_contrast);

    free(centers);
    free(indexes);
    return result_bitmap;
}

bitmap create_image_texture(bitmap image) {
    
    int number_of_centers, radius;
    v2 *centers = get_centers(image, &number_of_centers, &radius);
    if (settings.shuffle_centers) shuffle(centers, number_of_centers, 100); // Todo: get a better value for this

    Color *colors = compute_centers_colors(image, centers, number_of_centers, radius);

    float *brightnesses = (float *) malloc(sizeof(float) * number_of_centers);
    assert(brightnesses);
    float min_brightness = 255.0;
    float max_brightness = 0.0;
    for (int i = 0; i < number_of_centers; i++) {
        centers[i].x *= settings.scale;
        centers[i].y *= settings.scale;

        brightnesses[i] = (colors[i].R + colors[i].G + colors[i].B) / 3.0;

        float b = (float) settings.adjusted_brightness / 50.0;
        colors[i].R = clamp((int) (colors[i].R * b), 0, 255);
        colors[i].G = clamp((int) (colors[i].G * b), 0, 255);
        colors[i].B = clamp((int) (colors[i].B * b), 0, 255);

        if (settings.adjusted_hue != 0) colors[i] = shift_hue(colors[i], settings.adjusted_hue);

        max_brightness = MAX(max_brightness, brightnesses[i]);
        min_brightness = MIN(min_brightness, brightnesses[i]);
    }

    if (settings.centers_style == Random && settings.render_centers_by != Randomly) {
        sort_textures_by_birghtness(centers, colors, brightnesses, number_of_centers, settings.render_centers_by != Brightness);
    }
    
    bitmap result_bitmap;
    result_bitmap.Width  = image.Width  * settings.scale;
    result_bitmap.Height = image.Height * settings.scale;
    result_bitmap.Memory   = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    result_bitmap.Original = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    if (result_bitmap.Memory == NULL || result_bitmap.Original == NULL) {
        free(colors);
        free(centers);
        free(brightnesses);
        return result_bitmap;
    }
    
    if (!settings.use_original_as_background) {
        for (uint32 i = 0; i < result_bitmap.Width * result_bitmap.Height; i++) {
            result_bitmap.Memory[i*4 + 0] = settings.bg_color.B;
            result_bitmap.Memory[i*4 + 1] = settings.bg_color.G;
            result_bitmap.Memory[i*4 + 2] = settings.bg_color.R;
            result_bitmap.Memory[i*4 + 3] = settings.bg_color.A;
        }
    } else {
        blit_bitmap_to_bitmap(&result_bitmap, &image, 0, 0, result_bitmap.Width, result_bitmap.Height);
    }
    
    Texture *t = &textures[settings.texture_selected];
    int dim = 2 * radius * settings.scale;
    for (int i = 0; i < number_of_centers; i++) {
        // lower is less bright
        int index = 0;
        if (settings.size_style != Random_Size) {
            float b = (brightnesses[i] - min_brightness) / (max_brightness - min_brightness) * 255.0;
            index = b / 255.0 * (settings.range_high - 1 - settings.range_low) + settings.range_low;
            if (settings.size_style == Inverted_Size) index = settings.range_high - 1 + settings.range_low - index;
            index = clamp(index, settings.range_low, settings.range_high - 1);
        } else {
            index = rand() % (settings.range_high - 1 - settings.range_low) + settings.range_low;
            index = clamp(index, settings.range_low, settings.range_high - 1);
        }

        int w = dim;
        if (index >= t->elements) {
            assert(settings.allow_oversizing);
            w = dim * index / (t->elements - 1);
            index = t->elements - 1;
        }
        
        blit_texture_to_bitmap(&result_bitmap, index, centers[i], w, colors[i], settings.texture_selected);
    }

    memcpy(result_bitmap.Original, result_bitmap.Memory, result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    apply_brightness(result_bitmap, settings.result_brightness);
    apply_contrast(result_bitmap,   settings.result_contrast);

    free(centers);
    free(colors);
    free(brightnesses);

    return result_bitmap;
}

RECT compute_rendering_position(RECT dest_rect, int dest_side, int source_width, int source_height) {
    RECT result;

    result.left = dest_rect.left + dest_side;
    result.top  = dest_rect.top  + dest_side;
    int rect_width  = dest_rect.right  - dest_rect.left - dest_side * 2;
    int rect_height = dest_rect.bottom - dest_rect.top  - dest_side * 2;

    bool width_or_height = (float) rect_width / (float) source_width > (float) rect_height / (float) source_height;
    int rendered_width  = width_or_height ? rect_height * source_width / source_height : rect_width;
    int rendered_height = width_or_height ? rect_height : rect_width * source_height / source_width;

    result.right  = result.left + rendered_width;
    result.bottom = result.top  + rendered_height;

    return result;
}

inline bool v2_inside_rect(v2 v, RECT rect) {
    if (v.x < rect.left)   return false;
    if (v.x > rect.right)  return false;
    if (v.y < rect.top)    return false;
    if (v.y > rect.bottom) return false;
    return true;
}

// This needs some dll
bool open_file_externally(char *file_name, int size_file_name) {
    OPENFILENAME dialog_arguments;

    memset(&dialog_arguments, 0, sizeof(dialog_arguments));
    dialog_arguments.lStructSize = sizeof(dialog_arguments);
    dialog_arguments.hwndOwner = Window;
    dialog_arguments.lpstrFile = file_name;
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
    // use the contents of szFile to initialize itself.
    dialog_arguments.lpstrFile[0] = '\0';
    dialog_arguments.nMaxFile = size_file_name;
    dialog_arguments.lpstrFilter = "All\0*.*\0";
    dialog_arguments.nFilterIndex = 1;
    dialog_arguments.lpstrFileTitle = NULL;
    dialog_arguments.nMaxFileTitle = 0;
    dialog_arguments.lpstrInitialDir = NULL;
    dialog_arguments.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    return GetOpenFileName(&dialog_arguments);
}

bool strings_match(char *s1, char *s2, int length = -1) {
    if (length < 0) length = strlen(s1);

    for (int i = 0; i < length; i++)
        if (s1[i] != s2[i]) return false;

    return true;
}

int get_last_index(char *file_name, char item) {
    int result = 0;
    for (int i = 0; true; i++) {
        if (file_name[i] == '\0') return result;
        if (file_name[i] == item)  result = i;
    }
    return 0;
}

struct Slider_Handler {
    bool pressing_a_slider       = false;
    int  which_slider_is_pressed = -1;
    bool high_slider_pressed     = false; // for double sliders
};

Slider_Handler sliders;

struct Panel {
    int left_x = 0;

    int base_width  = 0;
    int base_height = 0;

    int row_height   = 0;
    int column_width = 0;

    int at_x = 0;
    int at_y = 0;

    int current_row    = 0;
    int current_column = 0;
    int columns_this_row = 0;

    bool adaptive_row = false;

    int font_size = Small_Font;

    void row(int columns = 1, float height_factor = 1, bool adaptive = false);
    void indent(float indent_percentage = 0.1);
    RECT get_current_rect();

    bool push_toggler(char *name, Color_Palette palette, bool *toggled, Color *override_color = NULL);
    int  push_updown_counter(char *name, Color_Palette palette, void *value, bool is_float = false);
    bool push_slider(char *text, Color_Palette palette, int *value, int min_v, int max_v, int slider_order);
    bool push_double_slider(char *text, Color_Palette palette, int *bottom_value, int *top_value, int min_v, int max_v, int slider_order, int extra_value);
    int  push_button(char *text, Color_Palette palette, int thickness = 2);
    bool push_header(char *text, Color_Palette palette, int header, int *current_header);
    int  push_selector(char *text, Color_Palette palette);
    void add_text(char *title, Color c, int override_font_size = -1);
};

enum Push_Result {
    Button_not_Pressed_nor_Hovered,
    Button_Hovered,
    Button_Left_Clicked,
    Button_Right_Clicked,
};

Panel make_panel(int x, int y, int height, int width, int font_size) {
    Panel result;

    result.left_x = x;
    result.at_x   = x;
    result.at_y   = y;

    result.base_width  = width;
    result.base_height = height;

    result.row_height   = 0;
    result.column_width = 0;

    result.font_size = font_size;
    return result;
}

RECT_f reset_zoom_rectangle(bitmap image, RECT render_rect) {

    float scale_w = (float) image.Width  / (float) get_w(render_rect);
    float scale_h = (float) image.Height / (float) get_h(render_rect);

    float w = scale_w > scale_h ? image.Width  : get_w(render_rect) * scale_h;
    float h = scale_w < scale_h ? image.Height : get_h(render_rect) * scale_w;

    float delta_x = (image.Width  - w) / 2;
    float delta_y = (image.Height - h) / 2;

    RECT_f result = {delta_x, delta_y, w + delta_x, h + delta_y};

    return result;
}

void update_zoom(RECT_f *zoom_rect, RECT render_rect, float delta, v2 zoom_center) {
    float w = get_w(*zoom_rect);
    float h = get_h(*zoom_rect);

    float click_x = zoom_center.x - render_rect.left;
    float click_y = zoom_center.y - render_rect.top;

    float percentage_x = click_x / (float) get_w(render_rect);
    float percentage_y = click_y / (float) get_h(render_rect);

    float hover_image_x = percentage_x * w + zoom_rect->left;
    float hover_image_y = percentage_y * h + zoom_rect->top;

    w *= (1 + delta);
    h = w * (float) get_h(render_rect) / (float) get_w(render_rect);

    zoom_rect->left   = hover_image_x - w * percentage_x;
    zoom_rect->right  = zoom_rect->left + w;
    zoom_rect->top    = hover_image_y - h * percentage_y;
    zoom_rect->bottom = zoom_rect->top + h;
}

int get_CPM() {
    LARGE_INTEGER counter_per_second;
    QueryPerformanceFrequency(&counter_per_second);
    return counter_per_second.QuadPart / 1000;
}

// Todo: if this fails we now quit, but we could keep going and just use the textures
void load_cap_image(bitmap *buffer, const char *file_name) {
    int Bpp;
    buffer->Memory = stbi_load(file_name, &buffer->Width, &buffer->Height, &Bpp, Bytes_Per_Pixel);
    assert(buffer->Memory);
    assert(Bpp == Bytes_Per_Pixel);

    swap_red_and_blue_channels(buffer);
    premultiply_alpha(buffer);
}

bool load_texture(int texture_index, char *file_name) {

    if (texture_index >= MAX_TEXTURES) return false;
    Texture *t = &textures[texture_index];
    if (t->texture.Memory) free(t->texture.Memory); // If the previous load failed we need to free the memory

    char *texture_prefix = "texture_";
    for (int i = 0; i < strlen(texture_prefix); i++) {
        if (texture_prefix[i] != file_name[i]) return false;
    }

    char file_path[file_name_size] = "Textures/";
    strcat(file_path, file_name);

    int Bpp;
    t->texture.Memory = stbi_load(file_path, &t->texture.Width, &t->texture.Height, &Bpp, Bytes_Per_Pixel);
    if (t->texture.Memory == NULL) return false;
    adjust_bpp(&t->texture, Bpp);

    t->element_side = t->texture.Height;
    t->elements     = t->texture.Width / t->element_side;
    if (t->elements == 0) return false;

    // If the background is white than we make sure the alpha is 0. This is because
    // it might not be simple to produce textures with alpha in them, for example if 
    // you are using Paint.
    uint32 *Pixels = (uint32 *) t->texture.Memory;
    for (int p = 0; p < t->texture.Width * t->texture.Height; p++) {
        uint8 B = (Pixels[p] >> 16) & 0xff;
        uint8 G = (Pixels[p] >>  8) & 0xff;
        uint8 R = (Pixels[p] >>  0) & 0xff;

        uint8 A = ((Pixels[p] >> 24) & 0xff);
        if (B == 255 && G == 255 && R == 255) A = 0;
        float A_scaled = (float) A / 255.0;

        B = B * A_scaled;
        G = G * A_scaled;
        R = R * A_scaled;

        Pixels[p] = (A << 24) | (B << 16) | (G << 8) | (R << 0);
    }

    // Remove "texture_" prefix
    file_name = file_name + strlen(texture_prefix);

    // Remove file extension
    int dot_index = get_last_index(file_name, '.');
    if (dot_index < 0) return false;
    file_name[dot_index] = '\0';

    strncpy(textures[total_textures].name, file_name, strlen(file_name));

    if (t->element_side * t->elements != t->texture.Width) {
        printf("Texture %s might be the wrong size\n", textures[total_textures].name);
    }

    return true;
}

bool save_file_into_memory(char *file_name, char *data, int file_size) {
    
    HANDLE file_handle;
    file_handle = CreateFile(file_name, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if(file_handle == INVALID_HANDLE_VALUE) {
        printf("ERROR: unable to create file handle\n");
        return false;
    }

    DWORD number_of_bytes_written, error;
    bool success = WriteFile(file_handle, data, file_size, &number_of_bytes_written, NULL);
    bool end_of_file_success = SetEndOfFile(file_handle);
    CloseHandle(file_handle);

    if(number_of_bytes_written != file_size) printf("Not everything was written!!\n");
    
    if (!success || !end_of_file_success) {
        error = GetLastError();
        printf("ERROR while reading the file %s. Error code: %d\n", file_name, error);
        return false;
    }
    return true;
}

char *load_file_memory(char *file_name, unsigned int *size) {
    char *buffer = NULL;
    DWORD number_of_bytes_read, high_file_size, error;
    HANDLE file_handle;

    file_handle = CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(file_handle == INVALID_HANDLE_VALUE) {
        printf("ERROR: unable to create file handle\n");
        return NULL;
    }

    *size = GetFileSize(file_handle, &high_file_size);

    buffer = (char *) malloc(*size);
    if (buffer == NULL) {
        printf("ERROR while allocating %d bytes ofmemory for loadign file: %s\n", *size, file_name);
        return 0;
    }
    bool result = ReadFile(file_handle, buffer, *size, &number_of_bytes_read, NULL);
    CloseHandle(file_handle);

    if (!result) {
        error = GetLastError();
        printf("ERROR while reading the file %s. Error code: %d\n", file_name, error);
    }
    return buffer;
}

char extension[] = ".settings";

void save_settings(General_Settings settings, char *full_file_name) {
    
    int slash_index = get_last_index(full_file_name, '\\');
    char file_name[file_name_size];
    strcpy(file_name, full_file_name + slash_index + 1);

    char *path = "Settings/*.settings";
    WIN32_FIND_DATA found_data;
    HANDLE settings_directory = FindFirstFile(path, &found_data);

    int matched_names = 0;
    do {
        if (found_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        
        char *settings_file_name = &found_data.cFileName[0];

        int dot_index = get_last_index(file_name, '.'); 
        if (dot_index + strlen(extension) > strlen(settings_file_name)) continue;

        bool match = strings_match(file_name, settings_file_name, dot_index - 1);
        if (!match) continue;
        
        int settings_dot_index = get_last_index(settings_file_name, '.');
        match = strings_match(extension, settings_file_name + settings_dot_index, strlen(extension));
        if (!match) continue;

        matched_names++;
    } while (FindNextFile(settings_directory, &found_data) != 0);
    
    FindClose(settings_directory);

    auto settings_directory_data = GetFileAttributes("Settings\\");
    if (settings_directory_data == INVALID_FILE_ATTRIBUTES) {
        char path_name[file_name_size];
        GetCurrentDirectory(file_name_size, path_name);
        strcat(path_name, "\\Settings");
        
        if (!CreateDirectoryA(path_name, NULL)) return;
    }

    char settings_file_name[strlen(file_name) + strlen(extension)] = "Settings\\";
    strcat(settings_file_name, file_name);
    settings_file_name[get_last_index(settings_file_name, '.')] = '\0';

    if (matched_names > 0) strcat(settings_file_name, to_string(matched_names).c_str());
    strcat(settings_file_name, ".settings");

    int size = sizeof(settings);
    save_file_into_memory(settings_file_name, (char *) &settings, size);
}

void load_settings() {
    char file_name[file_name_size];
    bool success = open_file_externally(file_name, file_name_size);

    int dot_index = get_last_index(file_name, '.');
    bool matched = strings_match(extension, file_name + dot_index, strlen(extension));
    if (!matched) return;

    unsigned int file_size;
    char *settings_data = load_file_memory(file_name, &file_size);
    if (settings_data == NULL) return;

    if (file_size != sizeof(General_Settings)) {
        free(settings_data);
        return;
    }

    memcpy(&settings, settings_data, sizeof(General_Settings));
    free(settings_data);
}

void eat_line(char *data, unsigned int size, unsigned int *at) {
    while (true) {
        if (*at >= size) return;
        if (data[*at] == '\n') {
            *at = *at + 1;
            return;
        }
        *at = *at + 1;
    }
}

void eat_whitespace(char *data, unsigned int size, unsigned int *at) {
    while (true) {
        if (*at >= size) return;

        if (data[*at] != '\n' && data[*at] != '\r' &&
            data[*at] != '\t' && data[*at] != '\v' &&
            data[*at] != '\f' &&data[*at] != ' ') return;

        *at = *at + 1;
    }
}

int maybe_parse_int(char *data, unsigned int size, unsigned int *at) {
    eat_whitespace(data, size, at);

    int number = 0;
    while (true) {
        if (data[*at] < '0') return number;
        if (data[*at] > '9') return number;
        
        int digit = data[*at] - '0';
        number = number * 10 + digit;

        *at = *at + 1;
    }
}

void load_and_initialize_caps_owned() {
    unsigned int size;
    auto data = load_file_memory("Caps\\Owned_Caps.txt", &size);
    if (data == NULL) return;

    unsigned int at = 0;
    while (true) {
        if (at >= size) return;

        switch (data[at]) {
            case '#': eat_line(data, size, &at); break;
            case '>': {
                at++;
                int cap_number = maybe_parse_int(data, size, &at);
                if (cap_number < 0 || cap_number >= Total_Caps) {
                    // When we fail parsing anything on this line, we just
                    // eat the full line and continue with the rest of the file.
                    eat_line(data, size, &at);
                    break;
                }
                
                if (at >= size) return; // Check 'at' before poking into memory
                if (data[at++] != ':') {
                    eat_line(data, size, &at);
                    break;
                }

                int number_of_caps_owned = maybe_parse_int(data, size, &at);
                if (number_of_caps_owned < 0) {
                    eat_line(data, size, &at);
                    break;
                }

                // We do a '- 1' because the name of the caps in the .pngs start
                // at 1 and the array starts at 0.
                caps_count[cap_number - 1] = number_of_caps_owned;
            } break;
        }

        // We eat all the spaces and black lines to make it more robust and easier to read
        eat_whitespace(data, size, &at);
    }

    free(data);
}

int main(void) {
    initialize_main_buffer();
    start_main_window();
    load_and_initialize_caps_owned();

    int CPM = get_CPM();
    ms t0, t1;
    get_time(&t0);

    memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
    blit_main_buffer_to_window();
    
    int sizes[N_SIZES] = {20, 35, 55};
    init_font(sizes);

    for (int i = 0; i < Total_Caps; i++) {
        string filepath = "Caps/Cap " + to_string(i + 1) + ".png";
        load_cap_image(&caps_data[i], filepath.c_str());
    }
    
    char *path = "Textures/*.*";
    WIN32_FIND_DATA ffd;
    HANDLE texture_dir = FindFirstFile(path, &ffd);

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        char *file_name = &ffd.cFileName[0];

        bool success = load_texture(total_textures, file_name);
        if (!success) continue;

        total_textures++;
        if (total_textures == MAX_TEXTURES) {
            printf("You have too many textures!");
            break;
        }
    } while (FindNextFile(texture_dir, &ffd) != 0);
    
    FindClose(texture_dir);
    assert(total_textures); // Todo: if no textures are detected we crash, but we could keep going with the caps

    char file_name[file_name_size] = "";
    int save_counter = 1; // Todo: start counter at the last already saved image +1
    bool saved_changes = true;

    bitmap image = {0};
    bitmap result_bitmap = {0};

    RECT_f source_zoom_rectangle, processed_zoom_rectangle;
    bool color_picker_active = false;

    while (true) {

        // Handle Messages
        auto result = handle_window_messages();
        if (result == -1) return 0;

        // Handle Inputs        
        POINT p;
        GetCursorPos(&p);
        mouse_position = screen_to_window_position(p);

        // Compute sizes
        int main_border  = 40;
        int inner_border = 20;

        int available_width = Main_Buffer.Width - main_border * 2 - inner_border * 2;
        available_width = clamp(available_width, 0, Main_Buffer.Width);
        int settings_width = clamp(available_width * 0.30, 0, 300);
        int display_width  = (available_width - settings_width) / 2;

        int available_height = Main_Buffer.Height - main_border * 2;
        float display_rows = (float) available_height / 40.0 - 3.5;

        int source_left    = main_border;
        int settings_left  = source_left   + display_width  + inner_border;
        int processed_left = settings_left + settings_width + inner_border;

        start_sliders();

        // Reset Background
        memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
        
        int border = 5;
        // Source Image Panel
        Panel source_panel = make_panel(source_left, main_border, 40, display_width, Medium_Font);
        source_panel.row(1, display_rows);

        RECT s_rect = source_panel.get_current_rect();
        RECT render_source_rect = {s_rect.left + border*2, s_rect.top + border*2, s_rect.right - border*2, s_rect.bottom - border*2};
        
        int source_button_result;
        if (image.Memory) {
            source_button_result = source_panel.push_button("", default_palette, border);
        } else {
            source_button_result = source_panel.push_button("Load image", default_palette, border);
        }
        source_panel.font_size = Small_Font;

        source_panel.row(1, 0.5);
        source_panel.row();
        bool s_change = source_panel.push_slider("Brightness", slider_palette, &settings.source_brightness, 1, 99, new_slider());
        source_panel.row();
        s_change |= source_panel.push_slider("Contrast", slider_palette, &settings.source_contrast, 1, 99, new_slider());
        if (s_change) {
            apply_brightness(image, settings.source_brightness, true);
            apply_contrast(image,   settings.source_contrast);
        }

        source_panel.row(2);
        if (source_panel.push_button("Swap R & B", default_palette, 2) == Button_Left_Clicked) {
            if (image.Memory) swap_red_and_blue_channels(&image);
        }

        if (source_panel.push_button("Crop", default_palette, 2) == Button_Left_Clicked) {
            if (image.Memory) {
                crop_image(&image, source_zoom_rectangle);
                source_zoom_rectangle = reset_zoom_rectangle(image, render_source_rect);
            }
        }

        // Processed Image Panel
        Panel result_panel = make_panel(processed_left, main_border, 40, display_width, Medium_Font);
        result_panel.row(1, display_rows);

        RECT p_rect = result_panel.get_current_rect();
        RECT render_processed_rect = {p_rect.left + border*2, p_rect.top + border*2, p_rect.right - border*2, p_rect.bottom - border*2};

        char process_text[] = "Process image";
        if (result_bitmap.Memory) process_text[0] = '\0';
        int processed_button_result = result_panel.push_button(process_text, default_palette, border);

        result_panel.font_size = Small_Font;  
        result_panel.row(1, 0.5);
        result_panel.row();
        bool r_change = result_panel.push_slider("Brightness", slider_palette, &settings.result_brightness, 1, 99, new_slider());
        result_panel.row();
        r_change |= result_panel.push_slider("Contrast", slider_palette, &settings.result_contrast, 1, 99, new_slider());
        if (r_change) {
            apply_brightness(result_bitmap, settings.result_brightness, true);
            apply_contrast(result_bitmap,   settings.result_contrast);
            if (result_bitmap.Memory) saved_changes = false;
        }

        result_panel.row(2);
        if (result_panel.push_button("Swap R & B", default_palette, 2) == Button_Left_Clicked) {
            if (result_bitmap.Memory) swap_red_and_blue_channels(&result_bitmap);
        }
        
        bool zoom_pressed = result_panel.push_toggler("Lock Zoom", default_palette, &settings.lock_zoom);
        if (zoom_pressed && settings.lock_zoom) {
            if (result_bitmap.Memory) processed_zoom_rectangle = reset_zoom_rectangle(result_bitmap, render_processed_rect);
            if (image.Memory)         source_zoom_rectangle    = reset_zoom_rectangle(image,         render_source_rect);
        }

        if (changed_size) {
            if (image.Memory) {
                v2 c = {(render_source_rect.right + render_source_rect.left) / 2, (render_source_rect.bottom + render_source_rect.top) / 2};
                update_zoom(&source_zoom_rectangle, render_source_rect, 0, c);
            }
            if (result_bitmap.Memory) {
                v2 c = {(render_processed_rect.right + render_processed_rect.left) / 2, (render_processed_rect.bottom + render_processed_rect.top) / 2};
                update_zoom(&processed_zoom_rectangle, render_processed_rect, 0, c);
            }
            changed_size = false;
        }

        // Process clicks, mousewheel and render bitmaps
        if (source_button_result == Button_Left_Clicked) {
            char temp_file_name[file_name_size];
            bool success = open_file_externally(temp_file_name, file_name_size);

            int Bpp;
            if (success) {
                if (image.Memory) free(image.Memory);
                image.Memory = stbi_load(temp_file_name, &image.Width, &image.Height, &Bpp, 4);
            }

            if (image.Memory && success) {
                adjust_bpp(&image, Bpp);
                premultiply_alpha(&image);

                if (image.Original) free(image.Original);
                image.Original = (uint8 *) malloc(image.Width * image.Height * Bytes_Per_Pixel);
                assert(image.Original);
                memcpy(image.Original, image.Memory, image.Width * image.Height * Bytes_Per_Pixel);

                memcpy(file_name, temp_file_name, file_name_size);

                source_zoom_rectangle = reset_zoom_rectangle(image, render_source_rect);
                compute_dimensions_from_radius(image);
                compute_dimensions_from_width(image);

                apply_brightness(image, settings.source_brightness);
                apply_contrast(image,   settings.source_contrast);
                
                save_counter = 1; // Todo: Not necessarily, find out which one it is
            }
        } else if (source_button_result == Button_Right_Clicked) {
            source_zoom_rectangle = reset_zoom_rectangle(image, render_source_rect);
            if (settings.lock_zoom) processed_zoom_rectangle = reset_zoom_rectangle(result_bitmap, render_processed_rect);
        }
        
        if (processed_button_result == Button_Left_Clicked) {
            if (image.Memory) {
                free(result_bitmap.Memory);
                free(result_bitmap.Original);

                if (settings.caps_or_texture == Caps_Style) {
                    if (settings.compute_with_limits) {
                        result_bitmap = create_image_caps_with_caps_limit(image);
                    } else {
                        result_bitmap = create_image_caps(image);
                    }
                } else {
                    assert(settings.caps_or_texture == Texture_Style);
                    result_bitmap = create_image_texture(image);
                }

                source_zoom_rectangle    = reset_zoom_rectangle(image, render_source_rect);
                processed_zoom_rectangle = reset_zoom_rectangle(result_bitmap, render_processed_rect);

                if (memcmp(&settings, &last_saved_settings, sizeof(General_Settings)) != 0) {
                    save_settings(settings, file_name);  
                    last_saved_settings = settings;
                }
                saved_changes = false;
            }
        } else if (processed_button_result == Button_Right_Clicked) {
            processed_zoom_rectangle = reset_zoom_rectangle(result_bitmap, render_processed_rect);
            if (settings.lock_zoom) source_zoom_rectangle = reset_zoom_rectangle(image, render_source_rect);
        }

        if (mousewheel_counter != 0) {
            float factor = -0.001;
            float delta  = factor * mousewheel_counter;

            if (source_button_result == Button_Hovered) {
                update_zoom(&source_zoom_rectangle, render_source_rect, delta, mouse_position);
                if (settings.lock_zoom) {
                    v2 zoom_center = mouse_position;
                    zoom_center.x += (render_processed_rect.left - render_source_rect.left);
                    update_zoom(&processed_zoom_rectangle, render_processed_rect, delta, zoom_center);
                }
            }
            if (processed_button_result == Button_Hovered) {
                update_zoom(&processed_zoom_rectangle, render_processed_rect, delta, mouse_position);
                if (settings.lock_zoom) {
                    v2 zoom_center = mouse_position;
                    zoom_center.x += (render_source_rect.left - render_processed_rect.left);
                    update_zoom(&source_zoom_rectangle, render_source_rect, delta, zoom_center);
                }
            }
            mousewheel_counter = 0;
        }
        
        if (image.Memory) {
            RECT zoom = rect_from_rectf(source_zoom_rectangle);
            render_bitmap_to_screen(&image, render_source_rect, zoom);
        }

        if (result_bitmap.Memory) {
            RECT zoom = rect_from_rectf(processed_zoom_rectangle);
            render_bitmap_to_screen(&result_bitmap, render_processed_rect, zoom);
        }

        // Settings Panel
        Panel settings_panel = make_panel(settings_left, main_border, 30, settings_width, Small_Font);
        settings_panel.row(2, 1.5, true);
        settings_panel.add_text("Settings", BLUE, Medium_Font);
        if (settings_panel.push_button("Load", default_palette) == Button_Left_Clicked) {
            load_settings();
        }

        settings_panel.row(1, 0.25);
        settings_panel.row(3);
        bool centers_header_pushed = false;
        centers_header_pushed |= settings_panel.push_header("Grid", header_palette, Grid,      &settings.centers_style);
        centers_header_pushed |= settings_panel.push_header("Hex",  header_palette, Hexagonal, &settings.centers_style);
        centers_header_pushed |= settings_panel.push_header("Rnd",  header_palette, Random,    &settings.centers_style);
        settings.centers_settings_visible = (settings.centers_settings_visible != centers_header_pushed);

        if (settings.centers_style == Hexagonal && settings.centers_settings_visible) {
            settings_panel.row(1, 0.25);
            settings_panel.row();
            int radius_press = settings_panel.push_updown_counter("Radius", default_palette, UpDown_(settings.radius));
            settings.radius = MAX(1, settings.radius + radius_press);
            if (radius_press != 0) compute_dimensions_from_radius(image);
            
            settings_panel.row();
            int x_hex_press = settings_panel.push_updown_counter("Horizontal", default_palette, UpDown_(settings.x_hex));
            settings.x_hex = MAX(1, settings.x_hex + x_hex_press);
            if (x_hex_press != 0) compute_dimensions_from_x_hex(image);
            
            settings_panel.row();
            int y_hex_press = settings_panel.push_updown_counter("Vertical", default_palette, UpDown_(settings.y_hex));
            settings.y_hex = MAX(1, settings.y_hex + y_hex_press);
            if (y_hex_press != 0) compute_dimensions_from_y_hex(image);

        } else if (settings.centers_style == Grid && settings.centers_settings_visible) {
            settings_panel.row(1, 0.25);
            settings_panel.row();
            int width_press = settings_panel.push_updown_counter("Width", default_palette, UpDown_(settings.width));
            settings.width = MAX(1, settings.width + width_press);
            if (width_press != 0) compute_dimensions_from_width(image);
            
            settings_panel.row();
            int x_grid_press = settings_panel.push_updown_counter("Horizontal", default_palette, UpDown_(settings.x_grid));
            settings.x_grid = MAX(1, settings.x_grid + x_grid_press);
            if (x_grid_press != 0) compute_dimensions_from_x_grid(image);
            
            settings_panel.row();
            int y_grid_press = settings_panel.push_updown_counter("Vertical", default_palette, UpDown_(settings.y_grid));
            settings.y_grid = MAX(1, settings.y_grid + y_grid_press);
            if (y_grid_press != 0) compute_dimensions_from_y_grid(image);

        } else if (settings.centers_style == Random && settings.centers_settings_visible) {
            settings_panel.row(1, 0.25);
            settings_panel.row();
            int random_radius_press = settings_panel.push_updown_counter("Radius", default_palette, UpDown_(settings.random_radius));
            settings.random_radius = MAX(1, settings.random_radius + random_radius_press);
            
            settings_panel.row();
            int total_element_press;
            if (settings.caps_or_texture == Caps_Style) {
                total_element_press = settings_panel.push_updown_counter("Caps", default_palette, UpDown_(settings.total_centers));
            } else {
                total_element_press = settings_panel.push_updown_counter("Elements", default_palette, UpDown_(settings.total_centers));
            }
            settings.total_centers = MAX(1, settings.total_centers + total_element_press * 10);

            settings_panel.row();
            settings_panel.add_text("Render order:", BLUE);

            settings_panel.row();
            int by_press;
            switch (settings.render_centers_by) {
                case Randomly:   by_press = settings_panel.push_selector("Randomly", default_palette);      break;
                case Brightness: by_press = settings_panel.push_selector("By Brightness", default_palette); break;
                case Darkness:   by_press = settings_panel.push_selector("By Darkness", default_palette);   break;
            }
            settings.render_centers_by += by_press;
            if (settings.render_centers_by == -1)                      settings.render_centers_by = Render_Centers_By_Count - 1;
            if (settings.render_centers_by == Render_Centers_By_Count) settings.render_centers_by = 0;
        }

        if (settings.centers_settings_visible) {
            settings_panel.row();
            float scale_press = settings_panel.push_updown_counter("Scale", default_palette, UpDown_(settings.scale), true);
            if (settings.scale >= 2) scale_press *= 0.5;
            else                     scale_press *= 0.1;
            settings.scale = clamp(settings.scale + scale_press, 0.1, 10);

            if (settings.centers_style != Random) {
                settings_panel.row();
                settings_panel.push_toggler("Shuffle Centers", default_palette, &settings.shuffle_centers);
            }
            
            settings_panel.row();
            settings_panel.push_slider("Brightness", slider_palette, &settings.adjusted_brightness, 1, 99, new_slider());

            settings_panel.row();
            settings_panel.push_slider("Hue Shift", slider_palette, &settings.adjusted_hue, -180, 180, new_slider());
        }
        
        settings_panel.row(1, 0.5);
        settings_panel.row(2);
        bool style_header_pushed = false;
        style_header_pushed |= settings_panel.push_header("Caps",    header_palette, Caps_Style,    &settings.caps_or_texture);
        style_header_pushed |= settings_panel.push_header("Texture", header_palette, Texture_Style, &settings.caps_or_texture);
        settings.style_settings_visible = (settings.style_settings_visible != style_header_pushed);

        if (settings.caps_or_texture == Caps_Style && settings.style_settings_visible) {
            settings_panel.row(1, 0.25);
            settings_panel.row();
            settings_panel.push_toggler("Invert", default_palette, &settings.inverse_caps);
            
            settings_panel.row();
            settings_panel.push_toggler("Compute with limits", default_palette, &settings.compute_with_limits);

            settings_panel.row(2);
            settings_panel.push_toggler("By Color", default_palette, &settings.by_color);
            
            if (settings.compute_with_limits) settings.by_color = true;
            if (settings.by_color) {
                settings_panel.push_toggler("Hard Max", default_palette, &settings.hard_max);
            }


        } else if (settings.caps_or_texture == Texture_Style && settings.style_settings_visible) {
            settings_panel.row(1, 0.25);
            
            settings_panel.row();
            int texture_press = settings_panel.push_selector(textures[settings.texture_selected].name, default_palette);
            
            if (texture_press != 0) {
                settings.texture_selected += texture_press;
                if (settings.texture_selected == -1)             settings.texture_selected = total_textures - 1;
                if (settings.texture_selected == total_textures) settings.texture_selected = 0;
            }

            settings_panel.row();
            settings_panel.push_toggler("Allow Oversizing", default_palette, &settings.allow_oversizing);

            int elements = textures[settings.texture_selected].elements;
            int max_range = (settings.allow_oversizing ? elements * 2: elements) - 1;
            int extra_value = settings.allow_oversizing ? elements - 1 : -1;
            settings.range_low  = clamp(settings.range_low,  0, max_range);
            settings.range_high = clamp(settings.range_high, 0, max_range);
            
            settings_panel.row(1, 0.25);
            settings_panel.row();
            settings_panel.push_double_slider("Range", slider_palette, &settings.range_low, &settings.range_high, 0, max_range, new_slider(), extra_value);
            settings_panel.row(1, 0.25);

            settings_panel.row();
            int size_press;
            switch (settings.size_style) {
                case Not_Inverted_Size: size_press = settings_panel.push_selector("Normal Size",   default_palette); break;
                case Inverted_Size:     size_press = settings_panel.push_selector("Inverted Size", default_palette); break;
                case Random_Size:       size_press = settings_panel.push_selector("Random Size",   default_palette); break;
            }
            settings.size_style += size_press;
            if (settings.size_style == -1)                  settings.size_style = Size_Settings_Count - 1;
            if (settings.size_style == Size_Settings_Count) settings.size_style = 0;
        }

        settings_panel.row(1, 0.5);
        settings_panel.row(3, 1, true);
        
        int selected_old = settings.show_settings;
        bool settings_header_pushed = false;
        settings_header_pushed |= settings_panel.push_header("Save",       header_palette, Show_Save_Settings,       &settings.show_settings);
        settings_header_pushed |= settings_panel.push_header("Gif",        header_palette, Show_Gif_Settings,        &settings.show_settings);
        settings_header_pushed |= settings_panel.push_header("Background", header_palette, Show_Background_Settings, &settings.show_settings);
        settings.show_settings_visible = (settings.show_settings_visible != settings_header_pushed);
        if (selected_old != settings.show_settings) settings.show_settings_visible = true;

        settings_panel.row(1, 0.5);
        if (settings.show_settings == Show_Save_Settings && settings.show_settings_visible) {
            settings_panel.row(1, 2);
            Color_Palette palette = saved_changes ? no_save_palette : save_palette;
            int save_click_result = settings_panel.push_button("Save", palette, 3);
            if (save_click_result == Button_Left_Clicked && result_bitmap.Memory) {
                int dot_index = get_last_index(file_name, '.');
                assert(dot_index >= 0);

                char save_file_name[file_name_size + 3];
                strncpy(save_file_name, file_name, dot_index);

                bool success = false;

                swap_red_and_blue_channels(&result_bitmap);
                if (settings.save_as_png) {
                    save_file_name[dot_index] = '\0';
                    strcat(save_file_name, to_string(save_counter).c_str());
                    strcat(save_file_name, ".png");
                    int pitch = Bytes_Per_Pixel * result_bitmap.Width;
                    success |= stbi_write_png(save_file_name, result_bitmap.Width, result_bitmap.Height, Bytes_Per_Pixel, result_bitmap.Memory, pitch);
                }
                
                if (settings.save_as_bmp) {
                    save_file_name[dot_index] = '\0';
                    strcat(save_file_name, to_string(save_counter).c_str());
                    strcat(save_file_name, ".bmp");
                    success |= stbi_write_bmp(save_file_name, result_bitmap.Width, result_bitmap.Height, Bytes_Per_Pixel, result_bitmap.Memory);
                }

                if (settings.save_as_jpg) {
                    save_file_name[dot_index] = '\0';
                    strcat(save_file_name, to_string(save_counter).c_str());
                    strcat(save_file_name, ".jpg");
                    success |= stbi_write_jpg(save_file_name, result_bitmap.Width, result_bitmap.Height, Bytes_Per_Pixel, result_bitmap.Memory, 100);
                }
                swap_red_and_blue_channels(&result_bitmap);

                if (success) {
                    save_counter++;
                    saved_changes = true;
                }
            }

            settings_panel.row(1, 0.25);
            settings_panel.row(3);
            settings_panel.push_toggler("png", default_palette, &settings.save_as_png);
            settings_panel.push_toggler("bmp", default_palette, &settings.save_as_bmp);
            settings_panel.push_toggler("jpg", default_palette, &settings.save_as_jpg);

        } else if (settings.show_settings == Show_Gif_Settings && settings.show_settings_visible) {
            settings_panel.row(1, 2);
            int gif_press = settings_panel.push_button("Make GIF", default_palette, 3);

            if (gif_press == Button_Left_Clicked && image.Memory) {

                int dot_index = get_last_index(file_name, '.');
                assert(dot_index >= 0);

                char save_file_name[file_name_size + 3];
                strncpy(save_file_name, file_name, dot_index);

                save_file_name[dot_index] = '\0';
                strcat(save_file_name, to_string(save_counter).c_str());
                strcat(save_file_name, ".gif");

                free(result_bitmap.Memory);
                free(result_bitmap.Original);

                if (settings.caps_or_texture == Caps_Style) {
                    result_bitmap = create_image_caps(image);
                } else {
                    assert(settings.caps_or_texture == Texture_Style);
                    result_bitmap = create_image_texture(image);
                }

                GifWriter writer = {};
                GifBegin(&writer, save_file_name, result_bitmap.Width, result_bitmap.Height, 20, 8, true);

                for (int i = 0; i < 16; i++) {
                    free(result_bitmap.Memory);
                    free(result_bitmap.Original);

                    if (settings.caps_or_texture == Caps_Style) {
                        result_bitmap = create_image_caps(image);
                    } else {
                        assert(settings.caps_or_texture == Texture_Style);
                        result_bitmap = create_image_texture(image);
                    }
                    swap_red_and_blue_channels(&result_bitmap);
                    GifWriteFrame(&writer, result_bitmap.Memory, result_bitmap.Width, result_bitmap.Height, 20, 8, true);
                }
                swap_red_and_blue_channels(&result_bitmap);

                GifEnd(&writer);
            }
        } else if (settings.show_settings == Show_Background_Settings && settings.show_settings_visible) {
            settings_panel.row(2, 1, true);
            settings_panel.push_toggler("Background", default_palette, &color_picker_active, &settings.bg_color);
            settings_panel.push_toggler("Original", default_palette, &settings.use_original_as_background);

            if (color_picker_active) {
                settings_panel.row(1, 0.1);            
                settings_panel.row();            
                RECT at_rect = settings_panel.get_current_rect();
                RECT color_picker_rect = {at_rect.left, at_rect.top, at_rect.right, at_rect.top + 170};
                render_filled_rectangle(color_picker_rect, BLACK);
                render_rectangle(color_picker_rect, WHITE, 5);

                Panel color_picker = make_panel(color_picker_rect.left + 10, color_picker_rect.top, 30, settings_width - 20, Small_Font);
                color_picker.row(1, 0.2);

                color_picker.row(1, 0.1);
                color_picker.row();
                color_picker.push_slider("R", slider_palette, &settings.bg_color.R, 0, 255, new_slider());
                color_picker.row();
                color_picker.push_slider("G", slider_palette, &settings.bg_color.G, 0, 255, new_slider());
                color_picker.row();
                color_picker.push_slider("B", slider_palette, &settings.bg_color.B, 0, 255, new_slider());
                color_picker.row();
                color_picker.push_slider("A", slider_palette, &settings.bg_color.A, 0, 255, new_slider());

                color_picker.row(2);
                if (color_picker.push_button("Reset", default_palette) == Button_Left_Clicked) settings.bg_color   = BLACK;
                if (color_picker.push_button("Done",  default_palette) == Button_Left_Clicked) color_picker_active = false;
            }
        }

        handled_press_left  = true;
        handled_press_right = true;
        blit_main_buffer_to_window();

        get_time(&t1);
        int ms_elapsed = time_elapsed(t0, t1, CPM);
        if (ms_elapsed < 33) Sleep(33 - ms_elapsed);
        t0 = t1;
    }
}

void Panel::row(int columns, float height_factor, bool adaptive) {
    if (columns < 1) columns = 1;

    current_row++;
    current_column = 1;
    columns_this_row = columns;

    // First you advance the at_y with the previous row_height
    at_y += row_height;
    at_x  = left_x;

    row_height   = base_height * height_factor;
    column_width = base_width / columns;
    adaptive_row = adaptive;
}

void Panel::indent(float indent_percentage) {
    int indent_by = indent_percentage * column_width;
    at_x += indent_by;
}

RECT Panel::get_current_rect() {
    return get_rect(at_x, at_y, column_width, row_height);
}

int get_width(int text_length, int font_size) {
    return text_length * (Font.advance * Font.scale[font_size]);
}

bool Panel::push_toggler(char *name, Color_Palette palette, bool *toggled, Color *override_color) {
    int thickness   = 2;
    int rect_side   = row_height - 2 * thickness;
    int inside_side = row_height - 6 * thickness;

    RECT button_rect = get_rect(at_x + thickness,     at_y + thickness,     rect_side, rect_side);
    RECT inside_rect = get_rect(at_x + thickness * 3, at_y + thickness * 3, inside_side, inside_side);

    int name_length = strlen(name);
    int name_width  = column_width - rect_side;
    if (adaptive_row) {
         if (current_column == columns_this_row) {
             name_width = left_x + base_width - at_x - rect_side;
             if (name_width < 0) name_width = 0;
         } else {
             name_width = get_width(name_length + 2, font_size);
         }
    }
    RECT name_rect = get_rect(at_x + rect_side, at_y, name_width, row_height);

    bool hovering = v2_inside_rect(mouse_position, button_rect) || v2_inside_rect(mouse_position, name_rect);
    bool highlighted = hovering && !sliders.pressing_a_slider;

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_rectangle(button_rect, button_color, thickness);
    if (override_color) render_filled_rectangle(inside_rect, *override_color);
    else if (*toggled)  render_filled_rectangle(inside_rect, button_color);
    render_text(name, name_length, font_size, name_rect, text_color);

    int toggler_width = rect_side + name_width;
    at_x += toggler_width;
    current_column++;

    if (highlighted && left_button_down && !handled_press_left) {
        *toggled = !(*toggled);
        return true;
    }
    return false;
}

int Panel::push_updown_counter(char *name, Color_Palette palette, void *value, bool is_float) {
    int thickness = 2;
    int rect_side = row_height - 2 * thickness;

    int name_length = strlen(name);

    RECT minus_rect = get_rect(at_x + thickness, at_y + thickness, rect_side,     rect_side);
    RECT value_rect = get_rect(minus_rect.right, at_y,             rect_side * 3, rect_side);
    RECT plus_rect  = get_rect(value_rect.right, at_y + thickness, rect_side,     rect_side);
    RECT name_rect  = get_rect(plus_rect.right,  at_y, (Font.advance * Font.scale[font_size]) * (name_length + 2), rect_side);

    bool highlighted = v2_inside_rect(mouse_position, get_current_rect()) && !sliders.pressing_a_slider;

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color value_color  = highlighted ? palette.highlight_value_color  : palette.value_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    char text[2] = "+";
    render_text(text, 1, font_size, plus_rect,  button_color);
    text[0] = '-';
    render_text(text, 1, font_size, minus_rect, button_color);

    render_rectangle(minus_rect, button_color, thickness);
    render_rectangle(plus_rect,  button_color, thickness);
    render_text(name, name_length, font_size, name_rect, text_color);

    if (is_float) render_text(*(float *)value, 2, font_size, value_rect, value_color);
    else          render_text(*(int *)  value,    font_size, value_rect, value_color);

    at_x += column_width;
    current_column++;

    if (left_button_down && !handled_press_left) {
        if (v2_inside_rect(mouse_position, minus_rect)) return -1;
        if (v2_inside_rect(mouse_position, plus_rect))  return +1;
    }

    if (right_button_down && !handled_press_right) {
        if (v2_inside_rect(mouse_position, minus_rect)) return -10;
        if (v2_inside_rect(mouse_position, plus_rect))  return +10;
    }

    return 0;
}

void Panel::add_text(char *text, Color c, int override_font_size) {
    int text_font_size = override_font_size != -1 ? override_font_size : font_size;
    
    int text_length = strlen(text);
    int text_width = column_width;
    if (adaptive_row) {
         if (current_column == columns_this_row) {
             text_width = left_x + base_width - at_x;
             if (text_width < 0) text_width = 0;
         } else {
             text_width = get_width(text_length + 2, text_font_size);
         }
    }
    RECT text_rect = get_rect(at_x, at_y, text_width, row_height);
    render_text(text, text_length, text_font_size, text_rect, c);

    at_x += text_width;
    current_column++;
}

bool Panel::push_header(char *text, Color_Palette palette, int header, int *current_header) {

    int text_length = strlen(text);
    int text_width = column_width;
    if (adaptive_row) {
         if (current_column == columns_this_row) {
             text_width = left_x + base_width - at_x;
             if (text_width < 0) text_width = 0;
         } else {
             text_width = get_width(text_length + 2, font_size);
         }
    }

    RECT text_rect = get_rect(at_x, at_y, text_width, row_height);
    bool highlighted = v2_inside_rect(mouse_position, text_rect) && !sliders.pressing_a_slider;

    Color text_color = palette.text_color;
    Color background_color = palette.background_color;
    if (header == (*current_header)) {
        background_color = palette.highlight_button_color;
        text_color = palette.highlight_text_color;
    } else if (highlighted) {
        background_color = palette.button_color;
        text_color = palette.highlight_text_color;
    }
    
    render_filled_rectangle(text_rect, background_color);
    render_text(text, strlen(text), font_size, text_rect, text_color);

    at_x += text_width;
    current_column++;

    if (highlighted && left_button_down && !handled_press_left) {
        if (*current_header == header) return true;
       *current_header = header;
    }
    return false;
}

int Panel::push_selector(char *text, Color_Palette palette) {
    int thickness = 2;
    int rect_side = row_height - 2 * thickness;

    RECT at = get_current_rect();
    RECT minus_rect = get_rect(at_x + thickness, at_y + thickness, rect_side, rect_side);
    RECT plus_rect  = get_rect(at.right - rect_side - thickness, at_y + thickness, rect_side, rect_side);
    RECT text_rect  = get_rect(minus_rect.right, at_y, plus_rect.left - minus_rect.right, row_height);

    bool highlighted = v2_inside_rect(mouse_position, at) && !sliders.pressing_a_slider;

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    char sign[2] = "<";
    render_text(sign, 1, font_size, minus_rect, button_color);
    sign[0] = '>';
    render_text(sign, 1, font_size, plus_rect,  button_color);

    render_rectangle(minus_rect, button_color, thickness);
    render_rectangle(plus_rect,  button_color, thickness);
    render_text(text, strlen(text), font_size, text_rect, text_color);
    
    at_x += column_width;
    current_column++;

    if (left_button_down && !handled_press_left) {
        if (v2_inside_rect(mouse_position, minus_rect)) return -1;
        if (v2_inside_rect(mouse_position, plus_rect))  return +1;
    }

    return 0;
}

int Panel::push_button(char *text, Color_Palette palette, int thickness) {

    int text_length = strlen(text);
    int width = column_width;
    if (adaptive_row) {
         if (current_column == columns_this_row) {
             width = left_x + base_width - at_x;
             if (width < 0) width = 0;
         } else {
             width = get_width(text_length + 2, font_size);
         }
    }

    RECT button_rect = get_rect(at_x, at_y, width, row_height);
    button_rect.left   += thickness;
    button_rect.right  -= thickness;
    button_rect.top    += thickness;
    button_rect.bottom -= thickness;

    bool highlighted = v2_inside_rect(mouse_position, button_rect) && !sliders.pressing_a_slider;

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_filled_rectangle(button_rect, palette.background_color);
    render_rectangle(button_rect, button_color, thickness);
    render_text(text, text_length, font_size, button_rect, text_color);

    at_x += column_width;
    current_column++;

    if (left_button_down  && !handled_press_left  && highlighted) return Button_Left_Clicked;
    if (right_button_down && !handled_press_right && highlighted) return Button_Right_Clicked;
    if (highlighted)                                              return Button_Hovered;

    return Button_not_Pressed_nor_Hovered;
}

bool slider_is_pressed(int index) {
    if (!sliders.pressing_a_slider)               return false;
    if (sliders.which_slider_is_pressed != index) return false;
    return true;
}
bool slider_is_pressed(int index, bool high) {
    if (!sliders.pressing_a_slider)               return false;
    if (sliders.which_slider_is_pressed != index) return false;
    if (sliders.high_slider_pressed     != high)  return false;

    return true;
}

bool Panel::push_slider(char *text, Color_Palette palette, int *value, int min_v, int max_v, int slider_order) {
    int text_length = strlen(text);
    int initial_value = *value;

    RECT slider_rect = get_rect(at_x,                    at_y, column_width / 2, row_height);
    RECT text_rect   = get_rect(at_x + column_width / 2, at_y, column_width / 2, row_height);
    
    int  thickness   = 2;
    int  slider_side = get_h(slider_rect) - thickness * 2;
    RECT line_rect = get_rect(slider_rect.left, (slider_rect.top + slider_rect.bottom - thickness) / 2, get_w(slider_rect), thickness);

    float pos = (float) (*value - min_v) / (float) (max_v - min_v);
    int slider_position = slider_rect.left + (get_w(slider_rect) - slider_side) * pos;
    RECT pos_rect = get_rect(slider_position, slider_rect.top + thickness, slider_side, slider_side);
    
    bool highlighted = (v2_inside_rect(mouse_position, get_current_rect()) || slider_is_pressed(slider_order)) && !(left_button_down && handled_press_left);
    if (sliders.pressing_a_slider && !slider_is_pressed(slider_order)) highlighted = false;
    
    Color slider_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color value_color  = highlighted ? palette.highlight_value_color  : palette.value_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_filled_rectangle(line_rect, palette.button_color);
    render_filled_rectangle(pos_rect,  slider_color);
    render_text(text, text_length, font_size, text_rect, text_color);
    
    if (!left_button_down) sliders.pressing_a_slider = false;
    else if (sliders.pressing_a_slider) {
        if (slider_is_pressed(slider_order)) {
            float new_pos = (float) (mouse_position.x - slider_rect.left) / get_w(slider_rect);
            new_pos = clamp(new_pos, 0, 1);
            *value = min_v + new_pos * (max_v - min_v);
        }
    } else if (!handled_press_left) {
        if (v2_inside_rect(mouse_position, slider_rect)) {
            sliders.pressing_a_slider = true;
            sliders.which_slider_is_pressed = slider_order;
            float new_pos = (float) (mouse_position.x - slider_rect.left) / get_w(slider_rect);
            new_pos = clamp(new_pos, 0, 1);
            *value = min_v + new_pos * (max_v - min_v);
        } else if (v2_inside_rect(mouse_position, text_rect)) {
            *value = (max_v + min_v) / 2;
        }
    }

    render_text(*value, Small_Font, pos_rect, value_color);

    at_x += column_width;
    current_column++;

    return initial_value != *value;
}

bool Panel::push_double_slider(char *text, Color_Palette palette, int *low_value, int *high_value, int min_v, int max_v, int slider_order, int extra_value) {
    int initial_high_value = *high_value;
    int initial_low_value  = *low_value;

    int text_length  = strlen(text);
    int text_width   = (text_length + 2) * Font.advance * Font.scale[font_size];
    int slider_width = column_width - text_width;

    RECT slider_rect = get_rect(at_x,                at_y, slider_width, row_height);
    RECT text_rect   = get_rect(at_x + slider_width, at_y, text_width,   row_height);

    bool highlighted = v2_inside_rect(mouse_position, get_current_rect()) && !(left_button_down && handled_press_left);
    
    int thickness = 2;
    int slider_height    = get_h(slider_rect) - thickness * 2;
    int slider_thickness = slider_height * 0.3;
    RECT line_rect = get_rect(slider_rect.left, (slider_rect.top + slider_rect.bottom - thickness) / 2, get_w(slider_rect), thickness);
    render_filled_rectangle(line_rect, palette.button_color);

    float high_pos = (float) (*high_value - min_v) / (float) (max_v - min_v);
    float low_pos  = (float) (*low_value  - min_v) / (float) (max_v - min_v);
    int high_slider_position = slider_rect.left + (get_w(slider_rect) - slider_thickness) * high_pos;
    int low_slider_position  = slider_rect.left + (get_w(slider_rect) - slider_thickness) * low_pos;

    if (extra_value > min_v) {
        float extra_pos = (float) (extra_value - min_v) / (float) (max_v - min_v);
        int extra_slider_position = slider_rect.left + (get_w(slider_rect) - slider_thickness) * extra_pos;
        RECT extra_pos_rect = get_rect(extra_slider_position, slider_rect.top + thickness, slider_thickness, slider_height);
        render_filled_rectangle(extra_pos_rect, palette.text_color);
    }

    RECT high_pos_rect = get_rect(high_slider_position, slider_rect.top + thickness, slider_thickness, slider_height);
    RECT low_pos_rect  = get_rect(low_slider_position,  slider_rect.top + thickness, slider_thickness, slider_height);

    Color high_slider_color = palette.button_color;
    Color low_slider_color  = palette.button_color;
    if (slider_is_pressed(slider_order,  true)) high_slider_color = palette.highlight_button_color;
    if (slider_is_pressed(slider_order, false)) low_slider_color  = palette.highlight_button_color;

    float value_0_to_1 = clamp((float) (mouse_position.x - slider_rect.left) / get_w(slider_rect), 0, 1);
    int value = min_v + value_0_to_1 * (max_v - min_v);
    bool high_is_closer = abs(value - *low_value) > abs(value - *high_value);

    if (!sliders.pressing_a_slider && highlighted) {
        if (high_is_closer) high_slider_color = palette.highlight_button_color;
        else                low_slider_color  = palette.highlight_button_color;
    }
    render_filled_rectangle(high_pos_rect, high_slider_color);
    render_filled_rectangle(low_pos_rect,  low_slider_color);
    
    Color text_color = highlighted || slider_is_pressed(slider_order) ? palette.highlight_text_color : palette.text_color;
    render_text(text, text_length, font_size, text_rect, text_color);

    if (!left_button_down) {
        sliders.pressing_a_slider = false;
    } else if (sliders.pressing_a_slider) {
        if (sliders.which_slider_is_pressed == slider_order) {
            if (sliders.high_slider_pressed) {
                *high_value = value;
                *high_value = MAX(*high_value, *low_value);
            } else {
                *low_value = value;
                *low_value = MIN(*low_value, *high_value);
            }
        }
    } else if (!handled_press_left) {
        if (v2_inside_rect(mouse_position, slider_rect)) {
            sliders.pressing_a_slider = true;
            sliders.which_slider_is_pressed = slider_order;
            if (high_is_closer) {
                sliders.high_slider_pressed = true;
                *high_value = value;
            } else {
                sliders.high_slider_pressed = false;
                *low_value = value;
            }
        } else if (v2_inside_rect(mouse_position, text_rect)) {
            *high_value = max_v;
            *low_value  = min_v;
        }
    }

    at_x += column_width;
    current_column++;

    return (initial_high_value != *high_value || initial_low_value != *low_value);
}