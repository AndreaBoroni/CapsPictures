#include <windows.h>
#include <string>
using namespace std;

/* Todo list:
    - Optimize
    - Don't draw text outside the window
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

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;

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

    BITMAPINFO *Info = NULL;
};

struct v2 {
    int x, y;
};

bitmap     Main_Buffer = {0};
BITMAPINFO Main_Info = {0};

HWND Window = {0};

// uint32 key_presses[100]  = {0};
// uint32 key_releases[100] = {0};
// int8 key_presses_length  = 0;
// int8 key_releases_length = 0;
bool left_button_down    = false;
bool right_button_down   = false;

int mousewheel_counter = 0;
v2  mousewheel_position = {0};

bool handled_press = false;
bool changed_size = false;

#define Total_Caps 121 // Don't know why only 99 load
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
        for (y = 0; y < quad.y1 - quad.y0; y++) {
            for (x = 0; x < quad.x1 - quad.x0; x++) {
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
            Source += (uint32) (Packed_Font_W     - (quad.s1 - quad.s0));
            Dest   += (uint32) (Main_Buffer.Width - (quad.x1 - quad.x0));
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
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            // uint32 VKCode = WParam;
            // bool WasDown = ((LParam & (1 << 30)) != 0); // Button released
            // bool IsDown  = ((LParam & (1 << 31)) == 0); // Buttom pressed

            // if (IsDown == WasDown) break;
            // if (WasDown) {
            //     key_releases[key_releases_length] = VKCode;
            //     key_releases_length++;
            // }
            // if (IsDown) {
            //     key_presses[key_presses_length] = VKCode;
            //     key_presses_length++;
            // }
        } break;
        case WM_LBUTTONDOWN:
            left_button_down = true;
            handled_press = false;
            break;
        case WM_LBUTTONUP:
            left_button_down = false;
            break;
        case WM_RBUTTONDOWN: right_button_down = true;  break;
        case WM_RBUTTONUP:   right_button_down = false; break;
        case WM_MOUSEWHEEL: {
            auto key_state = GET_KEYSTATE_WPARAM(WParam);
            auto delta     = GET_WHEEL_DELTA_WPARAM(WParam);

            if (key_state) break;

            mousewheel_counter += delta;

            POINT pos;
            pos.x = LOWORD(LParam);
            pos.y = HIWORD(LParam);
            mousewheel_position = screen_to_window_position(pos);

        } break;
        default:
            Result = DefWindowProc(Window, Message, WParam, LParam);
            break;
    }

    return Result;
}


#define INITIAL_WIDTH  1300
#define INITIAL_HEIGHT 800

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
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
        if (Message.message == WM_QUIT) return -1;
    }
    return 0;
}

#define max(a,b) ((a) > (b)) ? (a) : (b);
#define min(a,b) ((a) < (b)) ? (a) : (b);

void render_filled_rectangle(RECT rectangle, Color color)
{
    uint32 c = (color.A << 24) | (color.R << 16) | (color.G << 8) | color.B;
    
    int starting_x = max(rectangle.left,   0);
    int starting_y = max(rectangle.top,    0);
    int ending_x   = min(rectangle.right,  Main_Buffer.Width);
    int ending_y   = min(rectangle.bottom, Main_Buffer.Height);
    
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

    int starting_x = max(x, 0);
    int starting_y = max(y, 0);
    int ending_x   = min(x + width,  Dest->Width);
    int ending_y   = min(y + height, Dest->Height);
    
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

void render_bitmap_to_screen(bitmap *Source, int x, int y, int width, int height) {

    if (width  < 0) width  = Source->Width;
    if (height < 0) height = Source->Height;

    int starting_x = max(x, 0);
    int starting_y = max(y, 0);
    int ending_x   = min(x + width,  Main_Buffer.Width);
    int ending_y   = min(y + height, Main_Buffer.Height);
    
    if (starting_x > Main_Buffer.Width)  return;
    if (starting_y > Main_Buffer.Height) return;
    if (ending_x < 0) return;
    if (ending_y < 0) return;

    int x_bitmap, y_bitmap;

    int Dest_Pitch = Main_Buffer.Width * Bytes_Per_Pixel;
    uint8 *Row     = (uint8 *)  Main_Buffer.Memory;
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
    int starting_x = max(dest_rect.left, 0);
    int starting_y = max(dest_rect.top,  0);
    int ending_x   = min(dest_rect.right,  Main_Buffer.Width);
    int ending_y   = min(dest_rect.bottom, Main_Buffer.Height);
    
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

float compute_color_average(bitmap image, int center_x, int center_y, int radius, uint8 R_mask = 0xff, uint8 G_mask = 0xff, uint8 B_mask = 0xff) {
    int sum   = 0;
    int count = 0;

    int starting_x = max(center_x - radius, 0);
    int starting_y = max(center_y - radius, 0);
    int ending_x   = min(center_x + radius, image.Width);
    int ending_y   = min(center_y + radius, image.Height);
    
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

#define clamp(a, b, c) (((a) > (c)) ? (c) : (((a) < (b)) ? (b) : (a)))

struct Conversion_Parameters {
    int x_caps = 1;
    int y_caps = 1;
    int radius = 10;
    
    float scale = 1.0;

    bool inverse  = false;
    bool by_color = false;
    bool hard_max = false;

    bool save_as_png = true;
    bool save_as_bmp = false;
    bool save_as_jpg = false;
};

struct Zoom_Parameters {
    float zoom_level = 1.0;

    int center_x = 0;
    int center_y = 0;
};

void reset_zoom(Zoom_Parameters *zoom, bitmap *image = NULL) {
    zoom->zoom_level = 1.0;

    if (image) {
        zoom->center_x = image->Width  / 2;
        zoom->center_y = image->Height / 2;
    } else {
        zoom->center_x = 0;
        zoom->center_y = 0;
    }
}

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
        indexes[i] = (gray_value / 255.0) * (Total_Caps - 1);
        
        assert(gray_value >= 0);
        assert(indexes[i] >= 0);
        assert(indexes[i] < Total_Caps);

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
                distance = max((caps_colors[j].R - R_value) * (caps_colors[j].R - R_value), (caps_colors[j].G - G_value) * (caps_colors[j].G - G_value));
                distance = max((caps_colors[j].B - B_value) * (caps_colors[j].B - B_value), distance);
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

bitmap create_image(bitmap image, Conversion_Parameters param) {
    v2 *centers = (v2 *) malloc(sizeof(v2) * param.x_caps * param.y_caps);
    compute_centers(centers, param.x_caps, param.y_caps, param.radius);
    shuffle(centers, param.x_caps * param.y_caps, 1000); // Todo: make this an option

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
    result_bitmap.Memory = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);

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

    free(centers);
    return result_bitmap;
}

struct Button {
    RECT rect;
    int side;
    
    Color c;

    bool visible;
    int code;
};

struct Toggler {
    Button btn;
    bool toggled;

    RECT inside_rect;

    char *name;
    int   name_length;
    int   name_font_type;
    
    RECT  name_rect;
    Color name_color;
};

struct UpDown_Counter {
    RECT plus_rect, minus_rect;
    RECT number_rect;

    Color rect_color;
    Color plus_minus_color;
    Color text_color;

    char *name;
    int   name_length;
    RECT  name_rect;

    int minus_code;
    int plus_code;

    int font_type;
};

UpDown_Counter make_updown_counter(RECT minus_rect, RECT plus_rect, Color rect_c, Color plus_minus_c, Color text_c, char *name, int font_type) {
    UpDown_Counter result;

    result.minus_rect = minus_rect;
    result.plus_rect  = plus_rect;

    result.rect_color       = rect_c;
    result.plus_minus_color = plus_minus_c;
    result.text_color       = text_c;

    result.name_length = strlen(name);
    result.name = (char *) malloc(result.name_length);
    memcpy(result.name, name, result.name_length);
    
    result.font_type = font_type;

    result.number_rect = get_rect(minus_rect.right, minus_rect.top, plus_rect.left - minus_rect.right, get_h(minus_rect));
    result.name_rect   = result.number_rect;
    result.name_rect.top    -= get_h(minus_rect);
    result.name_rect.bottom -= get_h(minus_rect);

    return result;
}

Button make_button(RECT rect, int side, Color color, int code) {
    Button result;
    
    result.rect = rect;
    result.side = side;
    
    result.c       = color;
    result.visible = true;
    result.code    = code;
    
    return result;
}

Toggler make_toggler(Button btn, int where_text, char *name, Color name_color, int font_type) {
    Toggler result;

    result.btn = btn;
    result.toggled = false;

    result.inside_rect.left   = btn.rect.left   + btn.side * 2;
    result.inside_rect.right  = btn.rect.right  - btn.side * 2;
    result.inside_rect.top    = btn.rect.top    + btn.side * 2;
    result.inside_rect.bottom = btn.rect.bottom - btn.side * 2;

    result.name_length = strlen(name);
    result.name = (char *) malloc(result.name_length);
    memcpy(result.name, name, result.name_length);

    result.name_color  = name_color;
    result.name_font_type = font_type;

    int name_width = (result.name_length + 2) * Font.advance * Font.scale[font_type];
    switch (where_text) {
        case 0: // right
            result.name_rect = get_rect(btn.rect.right, btn.rect.top, name_width, get_h(btn.rect));
            break;
        case 1: // top 
            int left = (btn.rect.right + btn.rect.left - name_width) / 2;
            int top  = btn.rect.top - get_h(btn.rect);
            result.name_rect = get_rect(left, top, name_width, get_h(btn.rect));
            break;
    }

    return result;
}

void render_toggler(Toggler toggler) {
    render_rectangle(toggler.btn.rect, toggler.btn.c, toggler.btn.side);
    if (toggler.toggled) render_filled_rectangle(toggler.inside_rect, toggler.btn.c);
    render_text(toggler.name, toggler.name_length, Small_Font, toggler.name_rect, toggler.name_color);
}

void render_updown_counter(UpDown_Counter counter) {
    render_filled_rectangle(counter.minus_rect, counter.rect_color);
    render_filled_rectangle(counter.plus_rect,  counter.rect_color);
    
    char text[2] = "+";
    render_text(text, 1, Big_Font, counter.plus_rect,  counter.plus_minus_color);
    text[0] = '-';
    render_text(text, 1, Big_Font, counter.minus_rect, counter.plus_minus_color);

    render_text(counter.name, counter.name_length, counter.font_type, counter.name_rect, counter.text_color);
}

struct Error_Report {
    bool active;

    RECT rect;
    int  rect_side;

    Color rect_color;
    Color text_color;

    char text[260];
    int  length;

    int type;
};

enum Error_Types {
    LOADING_SOURCE,
    COMPUTING_RESULT,
    SAVING_RESULT,
};

Error_Report error = {0};

void render_error() {
    if (!error.active) return;

    render_rectangle(error.rect, error.rect_color, error.rect_side);
    render_text(error.text, error.length, Small_Font, error.rect, error.text_color);
}

void report_error(char *text, int type) {
    int length = strlen(text);
    memcpy(error.text, text, length);
    error.length = length;
    error.type   = type;
    error.active = true;

    error.rect.left   = 50;
    error.rect.top    = 700;
    error.rect.right  = error.rect.left + (length + 3) * Font.advance * Font.scale[Small_Font];
    error.rect.bottom = error.rect.top + 40;

    error.rect_side = 3;

    error.text_color = ERROR_RED;
    error.rect_color = WHITE;
}

void report_warning(char *text, int type) {
    report_error(text, type);
    error.text_color = WARNING;
}

void clear_error() { error.active = false; }
void clear_error(int type) { if (error.type == type) error.active = false; }

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

int button_pressed(Button *btn[], int btn_length, Toggler *tgl[], int tgl_length, UpDown_Counter *udc[], int udc_length) {

    if (!left_button_down || handled_press) return -1;

    POINT p;
    GetCursorPos(&p);
    v2 v = screen_to_window_position(p);

    for (int i = 0; i < btn_length; i++) {
        if (v2_inside_rect(v, btn[i]->rect)) return btn[i]->code;
    }
    
    for (int i = 0; i < tgl_length; i++) {
        if (v2_inside_rect(v, tgl[i]->btn.rect))  return tgl[i]->btn.code;
        if (v2_inside_rect(v, tgl[i]->name_rect)) return tgl[i]->btn.code;
    }

    for (int i = 0; i < udc_length; i++) {
        if (v2_inside_rect(v, udc[i]->plus_rect))  return udc[i]->plus_code;
        if (v2_inside_rect(v, udc[i]->minus_rect)) return udc[i]->minus_code;
    }

    return -1;
}

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

enum button_codes {
    SAVE_BTN,
    SOURCE_BTN,
    RESULT_BTN,
    INVERSE_BTN,
    BY_COLOR_BTN,
    HARD_MAX_BTN,
    RADIUS_PLUS,
    RADIUS_MINUS,
    XCAPS_PLUS,
    XCAPS_MINUS,
    YCAPS_PLUS,
    YCAPS_MINUS,
    SCALE_PLUS,
    SCALE_MINUS,
    PNG_BTN,
    BMP_BTN,
    JPG_BTN,
};

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

void shift_down_rect(RECT *rect, int by) { rect->top += by; rect->bottom += by; }

int main(void) {
    initialize_main_buffer();
    start_main_window();

    memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
    blit_main_buffer_to_window();
    
    int sizes[N_SIZES] = {20, 40, 60};
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
    char file_name[file_name_size] = "Courtois.jpg";
    int save_counter = 1; // Todo: start counter at the last already saved image +1
    bool saved_changes = true;

    bitmap image = {0};
    bitmap result_bitmap = {0};

    Conversion_Parameters param;
    Zoom_Parameters source_zoom, result_zoom;

    int btn_side        = 50;
    int plus_btn_x      = 700;
    int minus_btn_x     = 500;
    int starting_y      = 80;
    int vertical_offset = 100;

    Button source_image_btn = make_button({50, 50, 450, 600}, 5, LIGHT_GRAY, SOURCE_BTN);
    RECT   source_description_rect = get_rect(50, 600, 400, btn_side);
    
    Button result_image_btn = make_button({800, 50, 1200, 600}, 5, LIGHT_GRAY, RESULT_BTN);
    RECT   result_description_rect = get_rect(800, 600, 400, btn_side);

    RECT minus_rect, plus_rect;
    plus_rect  = get_rect(plus_btn_x,  starting_y, btn_side, btn_side);
    minus_rect = get_rect(minus_btn_x, starting_y, btn_side, btn_side);
    
    UpDown_Counter radius_counter = make_updown_counter(minus_rect, plus_rect, DARK_WHITE, BLACK, DARK_BLUE, "Radius", Medium_Font);
    RECT radius_value_rect = radius_counter.number_rect;
    radius_counter.plus_code  = RADIUS_PLUS;
    radius_counter.minus_code = RADIUS_MINUS;

    shift_down_rect(&plus_rect, vertical_offset); shift_down_rect(&minus_rect, vertical_offset);
    
    UpDown_Counter xcaps_counter = make_updown_counter(minus_rect, plus_rect, DARK_WHITE, BLACK, DARK_BLUE, "X Caps", Medium_Font);
    RECT xcaps_value_rect = xcaps_counter.number_rect;
    xcaps_counter.plus_code  = XCAPS_PLUS;
    xcaps_counter.minus_code = XCAPS_MINUS;
    
    shift_down_rect(&plus_rect, vertical_offset); shift_down_rect(&minus_rect, vertical_offset);

    UpDown_Counter ycaps_counter = make_updown_counter(minus_rect, plus_rect, DARK_WHITE, BLACK, DARK_BLUE, "Y Caps", Medium_Font);
    RECT ycaps_value_rect = ycaps_counter.number_rect;
    ycaps_counter.plus_code  = YCAPS_PLUS;
    ycaps_counter.minus_code = YCAPS_MINUS;

    shift_down_rect(&plus_rect, vertical_offset); shift_down_rect(&minus_rect, vertical_offset);

    UpDown_Counter scale_counter = make_updown_counter(minus_rect, plus_rect, DARK_WHITE, BLACK, DARK_BLUE, "Scale", Medium_Font);
    RECT scale_value_rect = scale_counter.number_rect;
    scale_counter.plus_code  = SCALE_PLUS;
    scale_counter.minus_code = SCALE_MINUS;
    
    int small_side      = btn_side / 2;
    int small_thickness = btn_side / 20;

    Toggler inverse_toggler;
    Button inverse_btn = make_button(get_rect(minus_btn_x, 450, small_side, small_side), small_thickness, DARK_WHITE, INVERSE_BTN);
    inverse_toggler = make_toggler(inverse_btn, 0, "Inverse", DARK_BLUE, Small_Font);
    
    Toggler by_color_toggler;
    Button by_color_btn = make_button(get_rect(minus_btn_x, 480, small_side, small_side), small_thickness, DARK_WHITE, BY_COLOR_BTN);
    by_color_toggler = make_toggler(by_color_btn, 0, "By Color", DARK_BLUE, Small_Font);
    
    Toggler hard_max_toggler;
    Button hard_max_btn = make_button(get_rect(minus_btn_x, 510, small_side, small_side), small_thickness, DARK_WHITE, HARD_MAX_BTN);
    hard_max_toggler = make_toggler(hard_max_btn, 0, "Hard Max", DARK_BLUE, Small_Font);
    
    Button fmt_btn[3];
    for (int i = 0; i < 3; i++) {
        int left = minus_btn_x + (plus_btn_x + btn_side - small_side - minus_btn_x) / 3 * i;
        fmt_btn[i] = make_button(get_rect(left, 540, small_side, small_side), small_thickness, DARK_WHITE, PNG_BTN + i);
    }

    Toggler png_toggler, bmp_toggler, jpg_toggler;
    png_toggler = make_toggler(fmt_btn[0], 0, "png", DARK_BLUE, Small_Font);
    bmp_toggler = make_toggler(fmt_btn[1], 0, "bmp", DARK_BLUE, Small_Font);
    jpg_toggler = make_toggler(fmt_btn[2], 0, "jpg", DARK_BLUE, Small_Font);

    Button save_btn = make_button({minus_btn_x, 650, plus_btn_x + btn_side, 650 + btn_side / 2 * 3}, 5, DARK_BLUE, SAVE_BTN);

    Button *all_buttons[100];
    int btn_length = 0;

    all_buttons[btn_length++] = &source_image_btn;
    all_buttons[btn_length++] = &result_image_btn;
    all_buttons[btn_length++] = &save_btn;

    Toggler *all_togglers[100];
    int tgl_length = 0;

    all_togglers[tgl_length++] = &inverse_toggler;
    all_togglers[tgl_length++] = &by_color_toggler;
    all_togglers[tgl_length++] = &hard_max_toggler;
    all_togglers[tgl_length++] = &png_toggler;
    all_togglers[tgl_length++] = &bmp_toggler;
    all_togglers[tgl_length++] = &jpg_toggler;

    UpDown_Counter *all_updown_counters[100];
    int udc_length = 0;

    all_updown_counters[udc_length++] = &radius_counter;
    all_updown_counters[udc_length++] = &xcaps_counter;
    all_updown_counters[udc_length++] = &ycaps_counter;
    all_updown_counters[udc_length++] = &scale_counter;
    
    while (true) {
        // Handle Messages
        auto result = handle_window_messages();
        if (result == -1) return 0;

        // Handle Inputs
        if (mousewheel_counter != 0) {
            if (v2_inside_rect(mousewheel_position, source_image_btn.rect)) {
                source_zoom.zoom_level += (float) mousewheel_counter / 50.0;
                source_zoom.zoom_level = clamp(source_zoom.zoom_level, 1, 100);
            }
            if (v2_inside_rect(mousewheel_position, result_image_btn.rect)) {
                result_zoom.zoom_level += (float) mousewheel_counter / 50.0;
                result_zoom.zoom_level = clamp(result_zoom.zoom_level, 1, 100);
            }
            mousewheel_counter = 0;
        }

        int pressed = button_pressed(all_buttons, btn_length, all_togglers, tgl_length, all_updown_counters, udc_length);
        switch (pressed) {
            case -1: break;
            case SAVE_BTN: {
                if (!result_bitmap.Memory) {
                    report_error("Nothing to save", SAVING_RESULT);
                    break;
                }

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
                    clear_error(SAVING_RESULT);
                    save_counter++;
                    saved_changes = true;
                } else {
                    report_error("Failed to save result", SAVING_RESULT);
                }                
            } break;
            case SOURCE_BTN: {
                char temp_file_name[file_name_size];
                
                bool success = open_file_externally(temp_file_name, file_name_size);
                if (!success) break;
                
                free(image.Memory);
                image.Memory = stbi_load(temp_file_name, &image.Width, &image.Height, &Bpp, 0);

                if (image.Memory) {
                    adjust_bpp(&image, Bpp);
                    // swap_red_and_blue_channels(&image);
                    premultiply_alpha(&image);

                    memcpy(file_name, temp_file_name, file_name_size);

                    compute_dimensions_from_radius(image, &param);
                    reset_zoom(&source_zoom, &image);

                    save_counter = 1; // Todo: Not necessarily
                    clear_error(LOADING_SOURCE);
                } else {
                    report_error("Failed to open file", LOADING_SOURCE);
                }

            } break;
            case RESULT_BTN: {
                if (image.Memory) {
                    free(result_bitmap.Memory);
                    result_bitmap = create_image(image, param);
                    reset_zoom(&result_zoom, &result_bitmap);
                    saved_changes = false;
                    clear_error(COMPUTING_RESULT);
                } else {
                    report_warning("Nothing to process", COMPUTING_RESULT);
                }
            } break;
            case RADIUS_PLUS: {
                param.radius++;
                compute_dimensions_from_radius(image, &param);
            } break;
            case RADIUS_MINUS: {
                param.radius--;
                if (param.radius == 0) param.radius = 1;
                compute_dimensions_from_radius(image, &param);
            } break;
            case XCAPS_PLUS: {
                param.x_caps++;
                compute_dimensions_from_x_caps(image, &param);
            } break;
            case XCAPS_MINUS: {
                param.x_caps--;
                if (param.x_caps == 0) param.x_caps = 1;
                compute_dimensions_from_x_caps(image, &param);
            } break;
            case YCAPS_PLUS: {
                param.y_caps++;
                compute_dimensions_from_y_caps(image, &param);
            } break;
            case YCAPS_MINUS: {
                param.y_caps--;
                if (param.y_caps == 0) param.y_caps = 1;
                compute_dimensions_from_y_caps(image, &param);
            } break;
            case SCALE_PLUS: {
                if (param.scale >= 2) param.scale += 0.5;
                else                  param.scale += 0.1;
            } break;
            case SCALE_MINUS: {
                if (param.scale >= 2.5) param.scale -= 0.5;
                else                    param.scale -= 0.1;
                if (param.scale <= 0.1) param.scale = 0.1;
            } break;
            case INVERSE_BTN:  param.inverse  = !param.inverse; break;
            case BY_COLOR_BTN: param.by_color = !param.by_color; break;
            case HARD_MAX_BTN: param.hard_max = !param.hard_max; break;
            case PNG_BTN: {
                param.save_as_png = !param.save_as_png;
            } break;
            case JPG_BTN: {
                param.save_as_jpg = !param.save_as_jpg;
            } break;
            case BMP_BTN: {
                param.save_as_bmp = !param.save_as_bmp;
            } break;
        }
        handled_press = true;

        // Render
        memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
        
        if (!saved_changes) render_filled_rectangle(save_btn.rect, BLUE);
        for (int i = 0; i < btn_length; i++) {
            render_rectangle(all_buttons[i]->rect, all_buttons[i]->c, all_buttons[i]->side);
        }

        // Todo: Find a way to do this better
        png_toggler.toggled = param.save_as_png;
        bmp_toggler.toggled = param.save_as_bmp;
        jpg_toggler.toggled = param.save_as_jpg;
        inverse_toggler.toggled  = param.inverse;
        by_color_toggler.toggled = param.by_color;
        hard_max_toggler.toggled = param.hard_max;

        for (int i = 0; i < tgl_length; i++) {
            render_toggler(*all_togglers[i]);
        }

        for (int i = 0; i < udc_length; i++) {
            render_updown_counter(*all_updown_counters[i]);
        }
        
        render_error();

        render_text(param.radius,   Medium_Font, radius_value_rect, BLUE);
        render_text(param.x_caps,   Medium_Font, xcaps_value_rect,  BLUE);
        render_text(param.y_caps,   Medium_Font, ycaps_value_rect,  BLUE);
        render_text(param.scale, 2, Medium_Font, scale_value_rect,  BLUE);

        char text[10];
        strncpy(text, "Save", 4);
        render_text(text, 4, Medium_Font, save_btn.rect, DARK_BLUE);
        strncpy(text, "Source", 6);
        render_text(text, 6, Big_Font, source_description_rect, DARK_BLUE);
        strncpy(text, "Processed", 9);
        render_text(text, 9, Big_Font, result_description_rect, DARK_BLUE);


        if (image.Memory) {
            RECT rect_s = compute_rendering_position(source_image_btn.rect, source_image_btn.side, image.Width, image.Height);
            RECT source_rect = get_rect(0, 0, image.Width, image.Height);

            source_rect.left   += (1.0 - 1.0 / source_zoom.zoom_level) * image.Width  / 2.0;
            source_rect.right  -= (1.0 - 1.0 / source_zoom.zoom_level) * image.Width  / 2.0;
            source_rect.top    += (1.0 - 1.0 / source_zoom.zoom_level) * image.Height / 2.0;
            source_rect.bottom -= (1.0 - 1.0 / source_zoom.zoom_level) * image.Height / 2.0;

            source_rect.left   += source_zoom.center_x - image.Width  / 2.0;
            source_rect.right  += source_zoom.center_x - image.Width  / 2.0;
            source_rect.top    += source_zoom.center_y - image.Height / 2.0;
            source_rect.bottom += source_zoom.center_y - image.Height / 2.0;

            render_bitmap_to_screen(&image, rect_s, source_rect);
        }

        if (result_bitmap.Memory) {
            RECT rect_r = compute_rendering_position(result_image_btn.rect, result_image_btn.side, result_bitmap.Width, result_bitmap.Height);
            RECT result_rect = get_rect(0, 0, result_bitmap.Width, result_bitmap.Height);
            
            result_rect.left   += (1.0 - 1.0 / result_zoom.zoom_level) * result_bitmap.Width  / 2.0;
            result_rect.right  -= (1.0 - 1.0 / result_zoom.zoom_level) * result_bitmap.Width  / 2.0;
            result_rect.top    += (1.0 - 1.0 / result_zoom.zoom_level) * result_bitmap.Height / 2.0;
            result_rect.bottom -= (1.0 - 1.0 / result_zoom.zoom_level) * result_bitmap.Height / 2.0;

            result_rect.left   += result_zoom.center_x - result_bitmap.Width  / 2.0;
            result_rect.right  += result_zoom.center_x - result_bitmap.Width  / 2.0;
            result_rect.top    += result_zoom.center_y - result_bitmap.Height / 2.0;
            result_rect.bottom += result_zoom.center_y - result_bitmap.Height / 2.0;

            render_bitmap_to_screen(&result_bitmap, rect_r, result_rect);
        }

        blit_main_buffer_to_window();

        Sleep(10);
    }
}
