#include <windows.h>
#include <string>
#include <stdlib.h>

using namespace std;

/* Todo list:
    - Optimize
    - Make button to swap red and blue
    - Save with the correct number
    - Clean up buttons generation
UI:
    - Better rendering in the window
    - Zoom in and out in the window
*/

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define UpDown_(var) ((void *)&(var))
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

    uint8 *Original = NULL;
    BITMAPINFO *Info = NULL;
};

struct v2 {
    int x, y;
};

bitmap     Main_Buffer = {0};
BITMAPINFO Main_Info = {0};

HWND Window = {0};

bool left_button_down    = false;
bool right_button_down   = false;

int mousewheel_counter = 0;
v2  mouse_position     = {0};

bool handled_press_left  = false;
bool handled_press_right = false;
bool changed_size = false;

#define Total_Caps 121
bitmap caps_data[Total_Caps];

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
    int R;
    int G;
    int B;
    int A;
};

const Color WHITE      = {255, 255, 255, 255};
const Color DARK_WHITE = {180, 180, 180, 255};
const Color LIGHT_GRAY = { 95,  95,  95, 255};
const Color GRAY       = { 50,  50,  50, 255};
const Color BLACK      = {  0,   0,   0, 255};
const Color GREEN      = { 70, 200,  80, 255};
const Color DARK_GREEN = { 50, 125,  60, 255};
const Color BLUE       = { 56, 185, 245, 255};
const Color DARK_BLUE  = {  5, 108, 156, 255};
const Color ERROR_RED  = {201,  36,  24, 255};
const Color WARNING    = {247, 194,  32, 255};

struct Color_Palette {
    Color button_color, highlight_button_color;
    Color value_color,  highlight_value_color;
    Color text_color,   highlight_text_color;
    Color background_color;
};

Color_Palette default_palette = {DARK_WHITE, WHITE, BLUE,  BLUE,  DARK_BLUE, BLUE, BLACK};
Color_Palette slider_palette  = {DARK_WHITE, WHITE, BLACK, BLACK, DARK_BLUE, BLUE, BLACK};
Color_Palette save_palette    = {DARK_BLUE,  BLUE,  BLACK, BLACK, BLUE,      BLUE, BLACK};

const int Packed_Font_W = 500;
const int Packed_Font_H = 500;

RECT get_rect(int x, int y, int width, int height) {
    RECT result = {x, y, x + width, y + height};
    return result;
}

void init_font(int sizes[])
{
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

void initialize_main_buffer()
{
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

void blit_main_buffer_to_window()
{
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

LRESULT CALLBACK main_window_callback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
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
            auto delta     = GET_WHEEL_DELTA_WPARAM(WParam);

            if (key_state) break;
            mousewheel_counter += delta;
        } break;
        default:
            Result = DefWindowProc(Window, Message, WParam, LParam);
            break;
    }

    return Result;
}

#define INITIAL_WIDTH  1100
#define INITIAL_HEIGHT 750

void start_main_window()
{
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

int get_w(RECT rect) { return rect.right - rect.left; }
int get_h(RECT rect) { return rect.bottom - rect.top; }

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

#define SQRT_2 1.4142
void compute_centers(v2 *centers, int x_caps, int y_caps, int radius) {
    int delta_vertical   = (SQRT_2 * radius) + 0.5;
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

float compute_color_average(bitmap image, int center_x, int center_y, int radius, uint8 R_mask = 0xff, uint8 G_mask = 0xff, uint8 B_mask = 0xff) {
    int sum   = 0;
    int count = 0;

    int starting_x = MAX(center_x - radius, 0);
    int starting_y = MAX(center_y - radius, 0);
    int ending_x   = MIN(center_x + radius, image.Width);
    int ending_y   = MIN(center_y + radius, image.Height);
    
    if (starting_x > image.Width)  return 0.0;
    if (starting_y > image.Height) return 0.0;
    if (ending_x < 0)              return 0.0;
    if (ending_y < 0)              return 0.0;

    int32 Pitch = image.Width * Bytes_Per_Pixel;
    uint8 *Row = image.Memory;
    Row += starting_x * Bytes_Per_Pixel + starting_y * Pitch;

    for (int Y = starting_y; Y < ending_y; Y++) {        
        uint32 *Pixel = (uint32 *)Row;   
        for (int X = starting_x; X < ending_x; X++) {
            uint8 R = (*Pixel >> 16) & R_mask;
            uint8 G = (*Pixel >>  8) & G_mask;
            uint8 B = (*Pixel >>  0) & B_mask;
            Pixel++;

            sum   += (R + G + B) / 3;
            count += ((*Pixel >> 24) & 0xff) / 255.0; // scale by the alpha value
        }
        Row += Pitch;
    }

    assert(sum >= 0);
    
    if (count == 0) return 0.0;
    else            return (float) sum / (float) count;
}

struct Conversion_Parameters {
    int x_caps = 1;
    int y_caps = 1;
    int radius = 10;
    
    float scale = 1.0;

    int result_brightness = 50;
    int result_contrast   = 50;

    int source_brightness = 50;
    int source_contrast   = 50;

    bool inverse        = false;
    bool by_color       = false;
    bool hard_max       = false;
    bool shuffle_caps   = false;
    bool random_centers = false;

    bool save_as_png = true;
    bool save_as_bmp = false;
    bool save_as_jpg = false;
};

void shuffle(v2 *array, int array_length, int shuffle_times)
{
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

int *compute_indexes(bitmap image, v2 *centers, Conversion_Parameters param) {

    int number_of_centers = param.x_caps * param.y_caps;
    int *indexes = (int *) malloc(sizeof(int) * number_of_centers);

    int min_index = Total_Caps - 1;
    int max_index = 0;

    for (int i = 0; i < number_of_centers; i++) {
        auto gray_value = compute_color_average(image, centers[i].x, centers[i].y, param.radius);
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
        if (param.inverse) indexes[i] = Total_Caps - indexes[i] - 1;
    }

    return indexes;
}

int *compute_indexes_by_color(bitmap image, v2 *centers, Conversion_Parameters param) {

    int number_of_centers = param.x_caps * param.y_caps;
    int *indexes = (int *) malloc(sizeof(int) * number_of_centers);

    Color caps_colors[Total_Caps];

    for (int i = 0; i < Total_Caps; i++) {
        caps_colors[i].R = compute_color_average(caps_data[i], 0, 0, caps_data[i].Width, 0xff, 0, 0);
        caps_colors[i].G = compute_color_average(caps_data[i], 0, 0, caps_data[i].Width, 0, 0xff, 0);
        caps_colors[i].B = compute_color_average(caps_data[i], 0, 0, caps_data[i].Width, 0, 0, 0xff);
    }

    for (int i = 0; i < number_of_centers; i++) {
        auto R_value = compute_color_average(image, centers[i].x, centers[i].y, param.radius, 0xff, 0, 0);
        auto G_value = compute_color_average(image, centers[i].x, centers[i].y, param.radius, 0, 0xff, 0);
        auto B_value = compute_color_average(image, centers[i].x, centers[i].y, param.radius, 0, 0, 0xff);

        float min_distance = 1;
        int   min_index    = -1;
        for (int j = 0; j < Total_Caps; j++) {
            float distance = (caps_colors[j].R - R_value) * (caps_colors[j].R - R_value) +
                             (caps_colors[j].G - G_value) * (caps_colors[j].G - G_value) +
                             (caps_colors[j].B - B_value) * (caps_colors[j].B - B_value);
            distance /= 3.0 * 255.0 * 255.0;
            
            if (param.hard_max) {
                distance = MAX((caps_colors[j].R - R_value) * (caps_colors[j].R - R_value), (caps_colors[j].G - G_value) * (caps_colors[j].G - G_value));
                distance = MAX((caps_colors[j].B - B_value) * (caps_colors[j].B - B_value), distance);
                distance /= 255.0 * 255.0;
            }

            if (distance <= min_distance) {
                min_distance = distance;
                min_index    = j;
            }
        }
        assert(min_index >= 0);
        indexes[i] = min_index;

        if (param.inverse) indexes[i] = Total_Caps - indexes[i] - 1;
    }

    return indexes;
}

void compute_dimensions_from_radius(bitmap image, Conversion_Parameters *param) {
    if (param->radius <= 0) param->radius = 1;
    param->x_caps = (float) image.Width  / (float) (2      * param->radius) + 1;
    param->y_caps = (float) image.Height / (float) (SQRT_2 * param->radius) + 1;
}


void compute_dimensions_from_x_caps(bitmap image, Conversion_Parameters *param) {  
    if (param->x_caps <= 0) param->x_caps = 1;
    param->radius = (float) image.Width / (float) (param->x_caps * 2);

    if (param->radius <= 0) param->radius = 1;
    param->y_caps = (float) image.Height / (float) (SQRT_2 * param->radius) + 1;
}

void compute_dimensions_from_y_caps(bitmap image, Conversion_Parameters *param) {
    if (param->y_caps <= 0) param->y_caps = 1;
    param->radius = (float) image.Width / (float) (param->y_caps * SQRT_2);

    if (param->radius <= 0) param->radius = 1;
    param->x_caps = (float) image.Height / (float) (2 * param->radius) + 1;
}

void adjust_bpp(bitmap *image, int Bpp) {
    if (Bpp == 4) return;
    
    if (Bpp == 3) {
        uint32 *new_Memory = (uint32 *) malloc(image->Width * image->Height * Bytes_Per_Pixel);
        uint8  *Bytes      = (uint8 *)  image->Memory;
        for (int p = 0; p < image->Width * image->Height; p++) {
            new_Memory[p] = (255 << 24) | (Bytes[p*3] << 16) | (Bytes[p*3 + 1] << 8) | (Bytes[p*3 + 2] << 0);
        }
        free(image->Memory);
        image->Memory = (uint8 *) new_Memory;
        return;
    }
    if (Bpp == 1) {
        uint32 *new_Memory = (uint32 *) malloc(image->Width * image->Height * Bytes_Per_Pixel);
        uint8  *Pixels     = (uint8 *)  image->Memory;
        for (int p = 0; p < image->Width * image->Height; p++) {
            new_Memory[p] = (255 << 24) | (Pixels[p] << 16) | (Pixels[p] << 8) | (Pixels[p] << 0);
        }
        free(image->Memory);
        image->Memory = (uint8 *) new_Memory;
        return;
    }
    assert(false);
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

    for (int p = 0; p < image.Width * image.Height; p++) {
        uint8 A = (source[p] >> 24);
        uint8 R = clamp((int) ((source[p] >> 16) & 0xff) + b, 0, 255);
        uint8 G = clamp((int) ((source[p] >>  8) & 0xff) + b, 0, 255);
        uint8 B = clamp((int) ((source[p] >>  0) & 0xff) + b, 0, 255);
        
        dest[p] = (A << 24) | (R << 16) | (G << 8) | (B << 0);
    }
}

void apply_contrast(bitmap image, int contrast, bool use_original_as_source = false) {
    if (contrast == 50 && !use_original_as_source) return;

    float scale = 255.0 / 49.0;
    int   c = (contrast - 50) * scale;
    float f = 259.0 / 255.0 * (255.0 + c) / (259.0 - c);

    uint32 *dest   = (uint32 *) image.Memory;
    uint32 *source = (uint32 *) image.Memory;
    if (use_original_as_source) source = (uint32 *) image.Original;

    for (int p = 0; p < image.Width * image.Height; p++) {
        int R_pre = (source[p] >> 16) & 0xff;
        int G_pre = (source[p] >>  8) & 0xff;
        int B_pre = (source[p] >>  0) & 0xff;

        uint8 A = ((source[p] >> 24) & 0xff);
        uint8 R = clamp(f * (R_pre - 128) + 128, 0, 255);
        uint8 G = clamp(f * (G_pre - 128) + 128, 0, 255);
        uint8 B = clamp(f * (B_pre - 128) + 128, 0, 255);
        
        dest[p] = (A << 24) | (R << 16) | (G << 8) | (B << 0);
    }

}

bitmap create_image(bitmap image, Conversion_Parameters param) {
    
    int number_of_centers = param.x_caps * param.y_caps;
    v2 *centers = (v2 *) malloc(sizeof(v2) * number_of_centers);

    if (param.random_centers) {
        // Todo better centers
        compute_centers_randomly(centers, number_of_centers, image.Width, image.Height);
    } else {
        compute_centers(centers, param.x_caps, param.y_caps, param.radius);
    }

    if (param.shuffle_caps) shuffle(centers, param.x_caps * param.y_caps, 100);

    int *indexes;
    if (param.by_color) indexes = compute_indexes_by_color(image, centers, param);
    else                indexes = compute_indexes(image, centers, param);

    int blit_radius = param.radius * param.scale;
    for (int i = 0; i < param.x_caps * param.y_caps; i++) {
        centers[i].x *= param.scale;
        centers[i].y *= param.scale;
    }
    
    bitmap result_bitmap;
    result_bitmap.Width  = image.Width  * param.scale;
    result_bitmap.Height = image.Height * param.scale;
    result_bitmap.Memory   = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    result_bitmap.Original = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);

    for (int i = 0; i < result_bitmap.Width * result_bitmap.Height; i++) {
        result_bitmap.Memory[i*4 + 0] = 0;
        result_bitmap.Memory[i*4 + 1] = 0;
        result_bitmap.Memory[i*4 + 2] = 0;
        result_bitmap.Memory[i*4 + 3] = 255;
    }
    
    int dim = 2 * blit_radius;
    for (int i = 0; i < param.x_caps * param.y_caps; i++) {
        blit_bitmap_to_bitmap(&result_bitmap, &caps_data[indexes[i]], centers[i].x - blit_radius, centers[i].y - blit_radius, dim, dim);
    }

    memcpy(result_bitmap.Original, result_bitmap.Memory, result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    apply_brightness(result_bitmap, param.result_brightness);
    apply_contrast(result_bitmap,   param.result_contrast);

    free(centers);
    free(indexes);
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

enum Push_Result {
    Button_not_Pressed_nor_Hovered,
    Button_Hovered,
    Button_Left_Clicked,
    Button_Right_Clicked,
};

int push_button(RECT rect, int side, Color_Palette palette, char *text = "", int font_size = 0) {
    bool left_press  = (left_button_down  && !handled_press_left);
    bool right_press = (right_button_down && !handled_press_right);
    bool hovering = v2_inside_rect(mouse_position, rect);

    Color button_color = hovering ? palette.highlight_button_color : palette.button_color;
    Color text_color   = hovering ? palette.highlight_text_color   : palette.text_color;

    render_filled_rectangle(rect, palette.background_color);
    render_rectangle(rect, button_color, side);
    render_text(text, strlen(text), font_size, rect, text_color);

    if (left_press  && hovering) return Button_Left_Clicked;
    if (right_press && hovering) return Button_Right_Clicked;
    if (hovering) return Button_Hovered;

    return Button_not_Pressed_nor_Hovered;
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

int get_dot_index(char *file_name, int file_name_size) {
    for (int i = 0; i < file_name_size; i++) {
        if (file_name[i] == '\0') {
            while (i >= 0) {
                if (file_name[i] == '.') return i;
                i--;
            }
        }
    }
    return -1;
}


struct Panel {
    int left_x = 0;

    int base_width  = 0;
    int base_height = 0;

    int row_height   = 0;
    int column_width = 0;

    int at_x = 0;
    int at_y = 0;

    int font_size = Small_Font;

    void row(int columns = 1, float height_factor = 1);
    void push_toggler(char *name, Color_Palette palette, bool *toggled);
    int  push_updown_counter(char *name, Color_Palette palette, void *value, bool is_float = false);
    bool push_slider(char *text, Color_Palette palette, int *value, int min_v, int max_v, int slider_order);
    bool push_button(char *text, Color_Palette palette);
    void add_title(char *title, Color c);
};

Panel make_panel(int x, int y, int height, int width, int font_size) {
    Panel result;

    result.left_x = x;
    result.at_x   = x;
    result.at_y   = y;

    result.base_width  = width;
    result.base_height = height;

    result.row_height = height;

    result.font_size = font_size;
    return result;
}

struct Slider_Handler {
    bool pressing_a_slider       = false;
    int  which_slider_is_pressed = -1;
};

Slider_Handler sliders;

RECT reset_zoom_rectangle(bitmap image, RECT render_rect) {

    float scale_w = (float) image.Width  / (float) get_w(render_rect);
    float scale_h = (float) image.Height / (float) get_h(render_rect);

    int w = scale_w > scale_h ? image.Width  : get_w(render_rect) * scale_h;
    int h = scale_w < scale_h ? image.Height : get_h(render_rect) * scale_w;

    int delta_x = (image.Width  - w) / 2;
    int delta_y = (image.Height - h) / 2;

    RECT result = {delta_x, delta_y, w + delta_x, h + delta_y};

    return result;
}

RECT update_zoom(RECT render_rect, RECT zoom_rect, float delta) {
    int w = get_w(zoom_rect);
    int h = get_h(zoom_rect);

    int click_x = mouse_position.x - render_rect.left;
    int click_y = mouse_position.y - render_rect.top;

    float percentage_x = (float) click_x / (float) get_w(render_rect);
    float percentage_y = (float) click_y / (float) get_h(render_rect);

    int hover_image_x = percentage_x * w + zoom_rect.left;
    int hover_image_y = percentage_y * h + zoom_rect.top;

    w *= (1 + delta);
    h *= (1 + delta);

    zoom_rect.left   = hover_image_x - w * percentage_x;
    zoom_rect.right  = hover_image_x + w * (1 - percentage_x);
    zoom_rect.top    = hover_image_y - h * percentage_y;
    zoom_rect.bottom = hover_image_y + h * (1 - percentage_y);

    return zoom_rect;
}

int get_CPM() {
    LARGE_INTEGER counter_per_second;
    QueryPerformanceFrequency(&counter_per_second);
    return counter_per_second.QuadPart / 1000;
}

int main(void) {
    initialize_main_buffer();
    start_main_window();

    int CPM = get_CPM();
    ms t0, t1;
    get_time(&t0);

    memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
    blit_main_buffer_to_window();
    
    int sizes[N_SIZES] = {20, 35, 55};
    init_font(sizes);

    int Bpp;
    for (int i = 0; i < Total_Caps; i++) {
        string filepath = "Caps/Cap " + to_string(i + 1) + ".png";
        caps_data[i].Memory = stbi_load(filepath.c_str(), &caps_data[i].Width, &caps_data[i].Height, &Bpp, Bytes_Per_Pixel);
        assert(caps_data[i].Memory);

        assert(Bpp == Bytes_Per_Pixel);

        swap_red_and_blue_channels(&caps_data[i]);
        premultiply_alpha(&caps_data[i]);
    }

    const int file_name_size = 400;
    char file_name[file_name_size] = "";
    int save_counter = 1; // Todo: start counter at the last already saved image +1
    bool saved_changes = true;

    bitmap image = {0};
    bitmap result_bitmap = {0};

    Conversion_Parameters param;
    RECT source_zoom_rectangle, processed_zoom_rectangle;

    while (true) {
        // Handle Messages
        auto result = handle_window_messages();
        if (result == -1) return 0;

        // Handle Inputs        
        POINT p;
        GetCursorPos(&p);
        mouse_position = screen_to_window_position(p);

        // Render
        memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
        
        RECT source_rect        = {50, 50, 400, 500};
        RECT render_source_rect = {source_rect.left + 5, source_rect.top + 5, source_rect.right - 5, source_rect.bottom - 5};
        int source_button_result = push_button(source_rect, 5, default_palette);
        if (source_button_result == Button_Left_Clicked) {
            char temp_file_name[file_name_size];
            bool success = open_file_externally(temp_file_name, file_name_size);

            if (success) {
                if (image.Memory) free(image.Memory);
                image.Memory = stbi_load(temp_file_name, &image.Width, &image.Height, &Bpp, 0);
            }

            if (image.Memory && success) {
                adjust_bpp(&image, Bpp);
                premultiply_alpha(&image);
                // swap_red_and_blue_channels(&image);

                if (image.Original) free(image.Original);
                image.Original = (uint8 *) malloc(image.Width * image.Height * Bytes_Per_Pixel);
                memcpy(image.Original, image.Memory, image.Width * image.Height * Bytes_Per_Pixel);

                memcpy(file_name, temp_file_name, file_name_size);

                source_zoom_rectangle = reset_zoom_rectangle(image, render_source_rect);
                compute_dimensions_from_radius(image, &param);

                apply_brightness(image, param.source_brightness);
                apply_contrast(image,   param.source_contrast);
                
                save_counter = 1; // Todo: Not necessarily, find out which one it is
            }
        } else if (source_button_result == Button_Right_Clicked) {
            source_zoom_rectangle = reset_zoom_rectangle(image, render_source_rect);
        }

        RECT processed_rect        = {650, 50, 1000, 500};
        RECT render_processed_rect = {processed_rect.left + 5, processed_rect.top + 5, processed_rect.right - 5, processed_rect.bottom - 5};
        int processed_button_result = push_button(processed_rect, 5, default_palette);
        if (processed_button_result == Button_Left_Clicked) {
            if (image.Memory) {
                free(result_bitmap.Memory);
                free(result_bitmap.Original);

                result_bitmap = create_image(image, param);
                processed_zoom_rectangle = reset_zoom_rectangle(result_bitmap, render_processed_rect);
                
                saved_changes = false;
            }
        } else if (processed_button_result == Button_Right_Clicked) {
            processed_zoom_rectangle = reset_zoom_rectangle(result_bitmap, render_processed_rect);
        }

        if (mousewheel_counter != 0) {
            float factor = -0.001;
            float delta  = factor * mousewheel_counter;

            if (source_button_result == Button_Hovered) {
                source_zoom_rectangle = update_zoom(render_source_rect, source_zoom_rectangle, delta);
            }
            if (processed_button_result == Button_Hovered) {
                processed_zoom_rectangle = update_zoom(render_processed_rect, processed_zoom_rectangle, delta);
            }

            mousewheel_counter = 0;
        }
        
        if (image.Memory) {
            render_bitmap_to_screen(&image, render_source_rect, source_zoom_rectangle);
        }

        if (result_bitmap.Memory) {
            render_bitmap_to_screen(&result_bitmap, render_processed_rect, processed_zoom_rectangle);
        }

        Panel settings_panel = make_panel(425, 50, 30, 200, Small_Font);
        settings_panel.add_title("Settings", BLUE);

        settings_panel.row();
        int radius_press = settings_panel.push_updown_counter("Radius", default_palette, UpDown_(param.radius));
        param.radius = MAX(1, param.radius + radius_press);
        if (radius_press != 0) compute_dimensions_from_radius(image, &param);
        
        settings_panel.row();
        int x_caps_press = settings_panel.push_updown_counter("X Caps", default_palette, UpDown_(param.x_caps));
        param.x_caps = MAX(1, param.x_caps + x_caps_press);
        if (x_caps_press != 0) compute_dimensions_from_x_caps(image, &param);
        
        settings_panel.row();
        int y_caps_press = settings_panel.push_updown_counter("Y Caps", default_palette, UpDown_(param.y_caps));
        param.y_caps = MAX(1, param.y_caps + y_caps_press);
        if (y_caps_press != 0) compute_dimensions_from_y_caps(image, &param);

        settings_panel.row();
        float scale_press = settings_panel.push_updown_counter("Scale", default_palette, UpDown_(param.scale), true);
        if (param.scale >= 2) scale_press *= 0.5;
        else                  scale_press *= 0.1;
        param.scale = MAX(0.1, param.scale + scale_press);

        settings_panel.row();
        settings_panel.row(3);
        settings_panel.push_toggler("png", default_palette, &param.save_as_png);
        settings_panel.push_toggler("bmp", default_palette, &param.save_as_bmp);
        settings_panel.push_toggler("jpg", default_palette, &param.save_as_jpg);

        settings_panel.row();
        settings_panel.push_toggler("Inverse", default_palette, &param.inverse);
        
        settings_panel.row();
        settings_panel.push_toggler("By Color", default_palette, &param.by_color);
        if (param.by_color) {
            settings_panel.row();
            settings_panel.push_toggler("Hard Max", default_palette, &param.hard_max);
        }
        
        settings_panel.row();
        settings_panel.push_toggler("Shuffle Caps", default_palette, &param.shuffle_caps);

        settings_panel.row();
        settings_panel.push_toggler("Random Centers", default_palette, &param.random_centers);

        settings_panel.row(1, 2);
        save_palette.background_color = saved_changes ? BLACK : WHITE;
        if (settings_panel.push_button("Save", save_palette) && result_bitmap.Memory) {

            int dot_index = get_dot_index(file_name, file_name_size);
            assert(dot_index >= 0);

            char save_file_name[file_name_size + 3];
            strncpy(save_file_name, file_name, dot_index);

            int success = 0;

            swap_red_and_blue_channels(&result_bitmap);
            if (param.save_as_png) {
                save_file_name[dot_index] = '\0';
                strcat(save_file_name, to_string(save_counter).c_str());
                strcat(save_file_name, ".png");
                success = stbi_write_png(save_file_name, result_bitmap.Width, result_bitmap.Height, Bytes_Per_Pixel, result_bitmap.Memory, Bytes_Per_Pixel * result_bitmap.Width);
            }
            
            if (param.save_as_bmp) {
                save_file_name[dot_index] = '\0';
                strcat(save_file_name, to_string(save_counter).c_str());
                strcat(save_file_name, ".bmp");
                success = stbi_write_bmp(save_file_name, result_bitmap.Width, result_bitmap.Height, Bytes_Per_Pixel, result_bitmap.Memory);
            }

            if (param.save_as_jpg) {
                save_file_name[dot_index] = '\0';
                strcat(save_file_name, to_string(save_counter).c_str());
                strcat(save_file_name, ".jpg");
                success = stbi_write_jpg(save_file_name, result_bitmap.Width, result_bitmap.Height, Bytes_Per_Pixel, result_bitmap.Memory, 100);
            }
            swap_red_and_blue_channels(&result_bitmap);

            if (success) {
                save_counter++;
                saved_changes = true;
            }
        }

        start_sliders();
        Panel source_panel = make_panel(50, 500, 40, 350, Medium_Font);
        source_panel.add_title("Settings", BLUE);

        source_panel.row();
        bool s_change = source_panel.push_slider("Brightness", slider_palette, &param.source_brightness, 1, 99, new_slider());
        source_panel.row();
        s_change |= source_panel.push_slider("Contrast", slider_palette, &param.source_contrast, 1, 99, new_slider());
        if (s_change) {
            apply_brightness(image, param.source_brightness, true);
            apply_contrast(image, param.source_contrast);
        }

        Panel result_panel = make_panel(650, 500, 40, 350, Medium_Font);
        result_panel.add_title("Processed", BLUE);

        result_panel.row();
        bool r_change = result_panel.push_slider("Brightness", slider_palette, &param.result_brightness, 1, 99, new_slider());
        result_panel.row();
        r_change |= result_panel.push_slider("Contrast", slider_palette, &param.result_contrast, 1, 99, new_slider());
        if (r_change) {
            apply_brightness(result_bitmap, param.result_brightness, true);
            apply_contrast(result_bitmap, param.result_contrast);
            if (result_bitmap.Memory) saved_changes = false;
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

void Panel::row(int columns, float height_factor) {
    at_y += row_height;
    at_x = left_x;

    // First you advance the at_y with the previous row_height
    row_height = base_height * height_factor;
    column_width = base_width / columns;
}

void Panel::push_toggler(char *name, Color_Palette palette, bool *toggled) {
    int thickness   = 2;
    int rect_side   = row_height - 2 * thickness;
    int inside_side = row_height - 6 * thickness;

    RECT button_rect = get_rect(at_x + thickness,     at_y + thickness,     rect_side, rect_side);
    RECT inside_rect = get_rect(at_x + thickness * 3, at_y + thickness * 3, inside_side, inside_side);

    int name_length = strlen(name);
    int name_width = MIN(column_width - rect_side, (name_length + 2) * (Font.advance * Font.scale[font_size]));
    RECT name_rect = get_rect(at_x + rect_side, at_y, name_width, row_height);

    bool highlighted = v2_inside_rect(mouse_position, button_rect) || v2_inside_rect(mouse_position, name_rect);

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_rectangle(button_rect, button_color, thickness);
    if (*toggled) render_filled_rectangle(inside_rect, button_color);
    render_text(name, name_length, font_size, name_rect, text_color);

    at_x += column_width;

    if (!left_button_down || handled_press_left) return;

    if (highlighted) *toggled = !(*toggled);
    return;
}

int Panel::push_updown_counter(char *name, Color_Palette palette, void *value, bool is_float) {
    int thickness = 2;
    int rect_side = row_height - 2 * thickness;

    int name_length = strlen(name);

    RECT minus_rect = get_rect(at_x + thickness, at_y + thickness, rect_side,     rect_side);
    RECT value_rect = get_rect(minus_rect.right, at_y,             rect_side * 3, rect_side);
    RECT plus_rect  = get_rect(value_rect.right, at_y + thickness, rect_side,     rect_side);
    RECT name_rect  = get_rect(plus_rect.right,  at_y, (Font.advance * Font.scale[font_size]) * (name_length + 2), rect_side);

    bool highlighted = v2_inside_rect(mouse_position, {minus_rect.left, minus_rect.top, name_rect.right, name_rect.bottom});

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
    else          render_text(*(int *)value, font_size, value_rect, value_color);

    at_x += column_width;

    if (!left_button_down || handled_press_left) return 0;

    if (v2_inside_rect(mouse_position, minus_rect)) return -1;
    if (v2_inside_rect(mouse_position, plus_rect))  return +1;

    return 0;
}

void Panel::add_title(char *title, Color c) {
    RECT title_rect = get_rect(at_x, at_y, base_width, base_height);
    render_text(title, strlen(title), clamp(font_size + 1, 0, N_SIZES - 1), title_rect, c);

    at_y += 0.5 * row_height;
}

bool Panel::push_button(char *text, Color_Palette palette) {
    int text_length  = strlen(text);
    int thickness = 2;

    RECT button_rect = get_rect(at_x, at_y, column_width, row_height);
    bool highlighted = v2_inside_rect(mouse_position, button_rect);

    Color button_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_filled_rectangle(button_rect, palette.background_color);
    render_rectangle(button_rect, button_color, thickness);
    render_text(text, text_length, Small_Font, button_rect, text_color);

    return highlighted && left_button_down && !handled_press_left;
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
    
    bool highlighted = v2_inside_rect(mouse_position, slider_rect) || v2_inside_rect(mouse_position, text_rect);
    
    Color slider_color = highlighted ? palette.highlight_button_color : palette.button_color;
    Color value_color  = highlighted ? palette.highlight_value_color  : palette.value_color;
    Color text_color   = highlighted ? palette.highlight_text_color   : palette.text_color;

    render_filled_rectangle(line_rect, slider_color);
    render_filled_rectangle(pos_rect,  slider_color);
    render_text(text, text_length, Small_Font, text_rect, text_color);
    
    if (!left_button_down) sliders.pressing_a_slider = false;
    else if (sliders.pressing_a_slider) {
        if (sliders.which_slider_is_pressed == slider_order) {
            float new_pos = (float) (mouse_position.x - slider_rect.left) / get_w(slider_rect);
            new_pos = clamp(new_pos, 0, 1);
            *value = min_v + new_pos * (max_v - min_v);
        }
    } else if (v2_inside_rect(mouse_position, slider_rect)) {
        sliders.pressing_a_slider = true;
        sliders.which_slider_is_pressed = slider_order;
        float new_pos = (float) (mouse_position.x - slider_rect.left) / get_w(slider_rect);
        new_pos = clamp(new_pos, 0, 1);
        *value = min_v + new_pos * (max_v - min_v);
    } else if (v2_inside_rect(mouse_position, text_rect)) {
        *value = (max_v + min_v) / 2;
    }

    render_text(*value, Small_Font, pos_rect, value_color);
    return initial_value != *value;
}