#include <windows.h>
#include <string>
using namespace std;

/* Todo list:
    - Save with the correct number
    - Blit background of the results with black
    - Save image in different file formats (.png, .bmp, .jpg)
    - Option to compute automatically at every param change
    - Clean up buttons generation
    - Get rid of the printfs
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
    uint8 *Memory;
    int32 Width;
    int32 Height;

    BITMAPINFO *Info;
};

bitmap     Main_Buffer = {0};
BITMAPINFO Main_Info = {0};

HWND Window = {0};

uint32 key_presses[100]  = {0};
uint32 key_releases[100] = {0};
int8 key_presses_length  = 0;
int8 key_releases_length = 0;
bool left_button_down    = false;
bool right_button_down   = false;

bool handled_press = false;
bool changed_size = false;

#define Total_Caps 121 // Don't know why only 99 load
bitmap caps_data[Total_Caps];

#define FIRST_CHAR_SAVED 32
#define LAST_CHAR_SAVED  126
#define CHAR_SAVED_RANGE LAST_CHAR_SAVED - FIRST_CHAR_SAVED + 1

struct Font_Data {
    int   size;
    float scale;
    
    int ascent;
    int descent;
    int baseline;
    
    int line_gap;
    
    int advance;
    int line_height;
    
    int vertical_offset;
    
    stbtt_fontinfo info;

    stbtt_pack_context *spc;
    stbtt_packedchar   chardata[CHAR_SAVED_RANGE];
};

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


const int Packed_Font_W = 1000;
const int Packed_Font_H = 400;

void init_font(int size, Font_Data *Font)
{
    char ttf_buffer[1<<20];
    Font->size = size;

    fread(ttf_buffer, 1, 1<<20, fopen("Font/consola.ttf", "rb"));

    stbtt_InitFont(&Font->info, (const uint8 *) ttf_buffer, stbtt_GetFontOffsetForIndex((const uint8 *) ttf_buffer, 0));
    stbtt_GetFontVMetrics(&Font->info, &(Font->ascent), &(Font->descent), &(Font->line_gap));

    Font->scale       = stbtt_ScaleForPixelHeight(&Font->info, Font->size);
    Font->baseline    = STBTT_iceil(Font->scale * Font->ascent); // Prevent the baseline to be rounded down (mem leaks)
    Font->line_height = (Font->ascent - Font->descent + Font->line_gap) * Font->scale;

    // Pack char data
    uint8 temp_bitmap[Packed_Font_W][Packed_Font_H];
    Font->spc = (stbtt_pack_context *) malloc(sizeof(stbtt_pack_context));

    stbtt_PackBegin(Font->spc, temp_bitmap[0], Packed_Font_W, Packed_Font_H, 0, 1, NULL);
    stbtt_PackSetOversampling(Font->spc, 1, 1);
    stbtt_PackFontRange(Font->spc, (const uint8 *) ttf_buffer, 0, size, FIRST_CHAR_SAVED, CHAR_SAVED_RANGE, Font->chardata);    
    stbtt_PackEnd(Font->spc);

    // Set advance and vertical_offset
    Font->advance         = 0;
    Font->vertical_offset = 0;
    for (char ch = FIRST_CHAR_SAVED; ch <= LAST_CHAR_SAVED; ch++) {
        int maybe_new_advance, maybe_new_offset;
        float xpos = 0;
        float ypos = Font->baseline;
        stbtt_aligned_quad quad;

        stbtt_GetCodepointHMetrics(&Font->info, ch, &maybe_new_advance, &maybe_new_offset); // last argument is not important
        stbtt_GetPackedQuad(Font->chardata, 1, 1, ch - FIRST_CHAR_SAVED, &xpos, &ypos, &quad, true);
        maybe_new_offset = (int)quad.y0;

        if (maybe_new_advance > Font->advance)         Font->advance         = maybe_new_advance;
        if (maybe_new_offset  < Font->vertical_offset) Font->vertical_offset = maybe_new_offset;
    }
    if (Font->vertical_offset > 0) Font->vertical_offset = 0;
    if (Font->vertical_offset < 0) Font->vertical_offset = -Font->vertical_offset;
}

void render_text(char *text, int length, Font_Data Font, RECT dest_rect, Color c = {255, 255, 255, 255}) {
    float xpos = (dest_rect.left + dest_rect.right) / 2 - (length * Font.advance * Font.scale) / 2;
    float ypos = dest_rect.top + Font.baseline - 5;
    stbtt_aligned_quad quad;

    int x, y;
    
    for (uint8 ch = 0; ch < length; ch++) {

        stbtt_GetPackedQuad(Font.chardata, 1, 1, text[ch] - FIRST_CHAR_SAVED, &xpos, &ypos, &quad, true);
        
        uint32 *Dest   = (uint32 *) Main_Buffer.Memory + (uint32) (Main_Buffer.Width * (quad.y0 + Font.vertical_offset) + quad.x0);
        uint8  *Source = Font.spc->pixels + (uint32) (Packed_Font_W * quad.t0 + quad.s0);
        for (y = 0; y < quad.y1 - quad.y0; y++) {
            for (x = 0; x < quad.x1 - quad.x0; x++) {
                float SA = *Source / 255.0;
                uint8 SR = c.R * SA;
                uint8 SG = c.G * SA;
                uint8 SB = c.B * SA;

                float DA = ((*Dest & 0xff000000) >> 24) / 255.0;
                uint8 DR = ((*Dest & 0x00ff0000) >> 16);
                uint8 DG = ((*Dest & 0x0000ff00) >>  8);
                uint8 DB = ((*Dest & 0x000000ff) >>  0);

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
        // if (xpos + Font.advance * Font.scale > Packed_Font_W) return;
    }
}

void render_text(int number, Font_Data Font, RECT dest_rect, Color c = {255, 255, 255, 255}) {
    char *text = (char *)to_string(number).c_str();
    
    int length;
    for (length = 0; text[length] != '\0'; length++) {}
    
    render_text(text, length, Font, dest_rect, c);
}

void render_text(float number, int digits, Font_Data Font, RECT dest_rect, Color c = {255, 255, 255, 255}) {
    char *text = (char *)to_string(number).c_str();
    
    int length;
    for (length = 0; text[length] != '\0'; length++) {}
    length = length < digits + 2 ? length : digits + 2;

    render_text(text, length, Font, dest_rect, c);
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
            uint32 VKCode = WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0); // Button released
            bool IsDown  = ((LParam & (1 << 31)) == 0); // Buttom pressed

            if (IsDown == WasDown) break;
            if (WasDown) {
                key_releases[key_releases_length] = VKCode;
                key_releases_length++;
            }
            if (IsDown) {
                key_presses[key_presses_length] = VKCode;
                key_presses_length++;
            }
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

            float SA = ((source_pixel & 0xff000000) >> 24) / 255.0;
            uint8 SB = ((source_pixel & 0x00ff0000) >> 16);
            uint8 SG = ((source_pixel & 0x0000ff00) >>  8);
            uint8 SR = ((source_pixel & 0x000000ff) >>  0);

            float DA = ((*Pixel & 0xff000000) >> 24) / 255.0;
            uint8 DB = ((*Pixel & 0x00ff0000) >> 16);
            uint8 DG = ((*Pixel & 0x0000ff00) >>  8);
            uint8 DR = ((*Pixel & 0x000000ff) >>  0);

            uint8 A = 255 * (SA + DA - SA*DA);
            uint8 R = DR * (1 - SA) + SR;
            uint8 G = DG * (1 - SA) + SG;
            uint8 B = DB * (1 - SA) + SB;

            *Pixel = (A << 24) | (B << 16) | (G << 8) | R;
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

            float SA = ((source_pixel & 0xff000000) >> 24) / 255.0;
            uint8 SB = ((source_pixel & 0x00ff0000) >> 16);
            uint8 SG = ((source_pixel & 0x0000ff00) >>  8);
            uint8 SR = ((source_pixel & 0x000000ff) >>  0);

            float DA = ((*Pixel & 0xff000000) >> 24) / 255.0;
            uint8 DR = ((*Pixel & 0x00ff0000) >> 16);
            uint8 DG = ((*Pixel & 0x0000ff00) >>  8);
            uint8 DB = ((*Pixel & 0x000000ff) >>  0);

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

struct v2 {
    int x, y;
};

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

float compute_gray_value(bitmap image, int center_x, int center_y, int radius) {
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
            uint8 R = (*Pixel >> 16) & 0xff;
            uint8 G = (*Pixel >>  8) & 0xff;
            uint8 B = (*Pixel >>  0) & 0xff;
            Pixel++;

            sum += (R + G + B) / 3;
            count++;
        }
        Row += Pitch;
    }

    assert(sum >= 0);
    
    if (count == 0) return 0.0;
    else            return (float) sum / (float) count;
}

int clamp(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

int *compute_indexes(bitmap image, v2 *centers, int number_of_centers, int radius) {

    int *indexes = (int *) malloc(sizeof(int) * number_of_centers);

    int min_index = Total_Caps - 1;
    int max_index = 0;

    for (int i = 0; i < number_of_centers; i++) {
        auto gray_value = compute_gray_value(image, centers[i].x, centers[i].y, radius);
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
    }

    return indexes;
}

struct conversion_parameters {
    int x_caps, y_caps;
    int radius;
    float scale;
};

void compute_dimensions_from_radius(bitmap image, conversion_parameters *param) {
    param->x_caps = (float) image.Width  / (float) (2      * param->radius) + 1;
    param->y_caps = (float) image.Height / (float) (SQRT_2 * param->radius) + 1;
}


void compute_dimensions_from_x_caps(bitmap image, conversion_parameters *param) {   
    param->radius = (float) image.Width / (float) (param->x_caps * 2);
    if (param->radius <= 0) param->radius = 1;
    param->y_caps = (float) image.Height / (float) (SQRT_2 * param->radius) + 1;
}

void compute_dimensions_from_y_caps(bitmap image, conversion_parameters *param) {
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

void premultiply_alpha(bitmap *image) {
    uint32 *Pixels = (uint32 *) image->Memory;
    for (int p = 0; p < image->Width * image->Height; p++) {
        uint8 A = ((Pixels[p] & 0xff000000) >> 24);
        float A_scaled = (float) A / 255.0;
        uint8 B = ((Pixels[p] & 0x00ff0000) >> 16) * A_scaled;
        uint8 G = ((Pixels[p] & 0x0000ff00) >>  8) * A_scaled;
        uint8 R = ((Pixels[p] & 0x000000ff) >>  0) * A_scaled;
        
        Pixels[p] = (A << 24) | (B << 16) | (G << 8) | (R << 0);
    }
}

bitmap create_image(bitmap image, conversion_parameters param) {
    v2 *centers = (v2 *) malloc(sizeof(v2) * param.x_caps * param.y_caps);
    compute_centers(centers, param.x_caps, param.y_caps, param.radius);

    int *indexes = compute_indexes(image, centers, param.x_caps * param.y_caps, param.radius);

    int blit_radius = param.radius * param.scale;
    for (int i = 0; i < param.x_caps * param.y_caps; i++) {
        centers[i].x *= param.scale;
        centers[i].y *= param.scale;
    }
    
    bitmap result_bitmap;
    result_bitmap.Width  = image.Width  * param.scale;
    result_bitmap.Height = image.Height * param.scale;
    result_bitmap.Memory = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);

    // TODO: look into having a black background for saving the images better
    // for (int i = 0; i < result_bitmap.Width * result_bitmap.Height; i++) result_bitmap.Memory[i*4] = 255;
    
    memset(result_bitmap.Memory, 0, result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);
    for (int i = 0; i < param.x_caps * param.y_caps; i++) {
        blit_bitmap_to_bitmap(&result_bitmap, &caps_data[indexes[i]], centers[i].x - blit_radius, centers[i].y - blit_radius, blit_radius * 2, blit_radius * 2);
    }

    free(centers);
    return result_bitmap;
}

struct button {
    RECT rect;
    int side;
    
    Color c;
    string text;

    bool visible;
    int code;

    bitmap bmp;
};

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

int button_pressed(button *btn[], int btn_length) {

    if (!left_button_down || handled_press) return -1;

    v2 v;
    POINT p;
    GetCursorPos(&p);

    RECT rect;
    GetClientRect( Window, (LPRECT) &rect);
    ClientToScreen(Window, (LPPOINT)&rect.left);
    ClientToScreen(Window, (LPPOINT)&rect.right);
    v.x = p.x - rect.left;
    v.y = p.y - rect.top;

    for (int i = 0; i < btn_length; i++) {
        if (v.x < btn[i]->rect.left)   continue;
        if (v.x > btn[i]->rect.right)  continue;
        if (v.y < btn[i]->rect.top)    continue;
        if (v.y > btn[i]->rect.bottom) continue;

        return btn[i]->code;
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
    COMPUTE_BTN,
    RADIUS_PLUS_BTN,
    RADIUS_MINUS_BTN,
    XCAPS_PLUS_BTN,
    XCAPS_MINUS_BTN,
    YCAPS_PLUS_BTN,
    YCAPS_MINUS_BTN,
    SCALE_PLUS_BTN,
    SCALE_MINUS_BTN,
};

RECT get_rect_from_dim(int x, int y, int width, int height) {
    RECT result = {x, y, x + width, y + height};
    return result;
}

int main(void) {
    initialize_main_buffer();
    start_main_window();

    memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
    blit_main_buffer_to_window();
    
    Font_Data Font;
    init_font(60, &Font);

    int Bpp;
    for (int i = 0; i < Total_Caps; i++) {
        string filepath = "Caps/Cap " + to_string(i + 1) + ".png";
        caps_data[i].Memory = stbi_load(filepath.c_str(), &caps_data[i].Width, &caps_data[i].Height, &Bpp, Bytes_Per_Pixel);
        if (caps_data[i].Memory == NULL) printf("ERROR loading file: %s\n", filepath.c_str());
        assert(Bpp == Bytes_Per_Pixel);

        premultiply_alpha(&caps_data[i]);
    }

    const int file_name_size = 400;
    char file_name[file_name_size] = "Courtois.jpg";
    int save_counter = 1; // Todo: start counter at the last already saved image +1
    bool saved_changes = true;

    bitmap image = {0};
    bitmap result_bitmap = {0};

    conversion_parameters param;
    param.radius = 20;
    param.scale  = 1;
    compute_dimensions_from_radius(image, &param);

    button *all_buttons[100];
    int btn_length = 0;

    button source_image_btn, result_image_btn;
    button save_btn;
    button radius_plus_btn, radius_minus_btn;
    button xcaps_plus_btn, xcaps_minus_btn;
    button ycaps_plus_btn, ycaps_minus_btn;
    button scale_plus_btn, scale_minus_btn;

    int btn_side = 50;
    int plus_btn_x  = 700;
    int minus_btn_x = 500;

    source_image_btn.rect = {50, 50, 450, 600};
    source_image_btn.side = 5;
    source_image_btn.c    = LIGHT_GRAY;
    source_image_btn.code = SOURCE_BTN;
    RECT source_description_rect = get_rect_from_dim(50, 600, 400, btn_side);
    
    result_image_btn.rect = {800, 50, 1200, 600};
    result_image_btn.side = 5;
    result_image_btn.c    = LIGHT_GRAY;
    result_image_btn.code = RESULT_BTN;
    RECT result_description_rect = get_rect_from_dim(800, 600, 400, btn_side);
    
    radius_plus_btn.rect = get_rect_from_dim(plus_btn_x, 100, btn_side, btn_side);
    radius_plus_btn.side = 100;
    radius_plus_btn.c    = DARK_WHITE;
    radius_plus_btn.code = RADIUS_PLUS_BTN;
    
    radius_minus_btn.rect = get_rect_from_dim(minus_btn_x, 100, btn_side, btn_side);
    radius_minus_btn.side = 100;
    radius_minus_btn.c    = DARK_WHITE;
    radius_minus_btn.code = RADIUS_MINUS_BTN;
    RECT radius_value_rect = {radius_minus_btn.rect.right, radius_plus_btn.rect.top, radius_plus_btn.rect.left, radius_plus_btn.rect.bottom};
    RECT radius_name_rect  = {radius_minus_btn.rect.right, radius_plus_btn.rect.top - btn_side * 1.2, radius_plus_btn.rect.left, radius_plus_btn.rect.top};

    xcaps_plus_btn.rect = get_rect_from_dim(plus_btn_x, 250, btn_side, btn_side);
    xcaps_plus_btn.side = 100;
    xcaps_plus_btn.c    = DARK_WHITE;
    xcaps_plus_btn.code = XCAPS_PLUS_BTN;
    
    xcaps_minus_btn.rect = get_rect_from_dim(minus_btn_x, 250, btn_side, btn_side);
    xcaps_minus_btn.side = 100;
    xcaps_minus_btn.c    = DARK_WHITE;
    xcaps_minus_btn.code = XCAPS_MINUS_BTN;
    RECT xcaps_value_rect = {xcaps_minus_btn.rect.right, xcaps_plus_btn.rect.top, xcaps_plus_btn.rect.left, xcaps_plus_btn.rect.bottom};
    RECT xcaps_name_rect  = {xcaps_minus_btn.rect.right, xcaps_plus_btn.rect.top - btn_side * 1.2, xcaps_plus_btn.rect.left, xcaps_plus_btn.rect.top};

    ycaps_plus_btn.rect = get_rect_from_dim(plus_btn_x, 400, btn_side, btn_side);
    ycaps_plus_btn.side = 100;
    ycaps_plus_btn.c    = DARK_WHITE;
    ycaps_plus_btn.code = YCAPS_PLUS_BTN;
    
    ycaps_minus_btn.rect = get_rect_from_dim(minus_btn_x, 400, btn_side, btn_side);
    ycaps_minus_btn.side = 100;
    ycaps_minus_btn.c    = DARK_WHITE;
    ycaps_minus_btn.code = YCAPS_MINUS_BTN;
    RECT ycaps_value_rect = {ycaps_minus_btn.rect.right, ycaps_plus_btn.rect.top, ycaps_plus_btn.rect.left, ycaps_plus_btn.rect.bottom};
    RECT ycaps_name_rect  = {ycaps_minus_btn.rect.right, ycaps_plus_btn.rect.top - btn_side * 1.2, ycaps_plus_btn.rect.left, ycaps_plus_btn.rect.top};
    
    scale_plus_btn.rect = get_rect_from_dim(plus_btn_x, 550, btn_side, btn_side);
    scale_plus_btn.side = 100;
    scale_plus_btn.c    = DARK_WHITE;
    scale_plus_btn.code = SCALE_PLUS_BTN;
    
    scale_minus_btn.rect = get_rect_from_dim(minus_btn_x, 550, btn_side, btn_side);
    scale_minus_btn.side = 100;
    scale_minus_btn.c    = DARK_WHITE;
    scale_minus_btn.code = SCALE_MINUS_BTN;
    RECT scale_value_rect = {scale_minus_btn.rect.right, scale_plus_btn.rect.top, scale_plus_btn.rect.left, scale_plus_btn.rect.bottom};
    RECT scale_name_rect  = {scale_minus_btn.rect.right, scale_plus_btn.rect.top - btn_side * 1.2, scale_plus_btn.rect.left, scale_plus_btn.rect.top};

    save_btn.rect = get_rect_from_dim(minus_btn_x, 650, plus_btn_x - minus_btn_x + btn_side, btn_side);
    save_btn.side = 100;
    save_btn.c    = BLUE;
    save_btn.code = SAVE_BTN;

    all_buttons[btn_length++] = &source_image_btn;
    all_buttons[btn_length++] = &result_image_btn;
    all_buttons[btn_length++] = &save_btn;
    all_buttons[btn_length++] = &radius_plus_btn;
    all_buttons[btn_length++] = &radius_minus_btn;
    all_buttons[btn_length++] = &xcaps_plus_btn;
    all_buttons[btn_length++] = &xcaps_minus_btn;
    all_buttons[btn_length++] = &ycaps_plus_btn;
    all_buttons[btn_length++] = &ycaps_minus_btn;
    all_buttons[btn_length++] = &scale_plus_btn;
    all_buttons[btn_length++] = &scale_minus_btn;
    
    while (true) {
        // Handle Messages
        auto result = handle_window_messages();
        if (result == -1) return 0;

        // Handle Inputs
        int pressed = button_pressed(all_buttons, btn_length);
        switch (pressed) {
            case -1: break;
            case SAVE_BTN: {
                int dot_index = -1;
                for (int i = 0; i < file_name_size; i++) {
                    if (file_name[i] == '\0') {
                        while (i >= 0) {
                            if (file_name[i] == '.') {
                                dot_index = i;
                                break;       
                            }
                            i--;
                        }
                        break;
                    }
                }
                assert(dot_index >= 0);

                char save_file_name[file_name_size + 3];

                strncpy(save_file_name, file_name, dot_index);
                save_file_name[dot_index] = '\0';
                strcat(save_file_name, to_string(save_counter).c_str());
                strcat(save_file_name, file_name + dot_index);

                int success = stbi_write_png(save_file_name, result_bitmap.Width, result_bitmap.Height, Bytes_Per_Pixel, result_bitmap.Memory, Bytes_Per_Pixel * result_bitmap.Width);
                if (success == 0) {
                    printf("Failed to write image to file!!\n");
                    break;
                }
                save_counter++;
                saved_changes = true;
            } break;
            case SOURCE_BTN: {
                char temp_file_name[file_name_size];
                
                bool success = open_file_externally(temp_file_name, file_name_size);
                if (!success) break;
                
                free(image.Memory);
                image.Memory = stbi_load(temp_file_name, &image.Width, &image.Height, &Bpp, 0);

                if (image.Memory) {
                    adjust_bpp(&image, Bpp);
                    premultiply_alpha(&image);

                    compute_dimensions_from_radius(image, &param);

                    memcpy(file_name, temp_file_name, file_name_size);
                    save_counter = 1;
                } else {
                    printf("Unable to open file!\n");
                }

            } break;
            case RESULT_BTN: {
                if (image.Memory) {
                    free(result_bitmap.Memory);
                    result_bitmap = create_image(image, param);
                    saved_changes = false;
                }
            } break;
            case RADIUS_PLUS_BTN: {
                param.radius++;
                compute_dimensions_from_radius(image, &param);
            } break;
            case RADIUS_MINUS_BTN: {
                param.radius--;
                if (param.radius == 0) param.radius = 1;
                compute_dimensions_from_radius(image, &param);
            } break;
            case XCAPS_PLUS_BTN: {
                param.x_caps++;
                compute_dimensions_from_x_caps(image, &param);
            } break;
            case XCAPS_MINUS_BTN: {
                param.x_caps--;
                if (param.x_caps == 0) param.x_caps = 1;
                compute_dimensions_from_x_caps(image, &param);
            } break;
            case YCAPS_PLUS_BTN: {
                param.y_caps++;
                compute_dimensions_from_y_caps(image, &param);
            } break;
            case YCAPS_MINUS_BTN: {
                param.y_caps--;
                if (param.y_caps == 0) param.y_caps = 1;
                compute_dimensions_from_y_caps(image, &param);
            } break;
            case SCALE_PLUS_BTN: {
                param.scale += 0.1;
            } break;
            case SCALE_MINUS_BTN: {
                param.scale -= 0.1;
                if (param.scale <= 0.1) param.scale = 0.1;
            } break;
        }
        handled_press = true;

        // Render
        memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
        for (int i = 0; i < btn_length; i++) {
            render_rectangle(all_buttons[i]->rect, all_buttons[i]->c, all_buttons[i]->side);
            if (all_buttons[i]->code == SCALE_PLUS_BTN || all_buttons[i]->code == XCAPS_PLUS_BTN ||
                all_buttons[i]->code == YCAPS_PLUS_BTN || all_buttons[i]->code == RADIUS_PLUS_BTN) {
                char text[2] = "+";
                render_text(text, 1, Font, all_buttons[i]->rect, BLACK);
            }
            
            if (all_buttons[i]->code == SCALE_MINUS_BTN || all_buttons[i]->code == XCAPS_MINUS_BTN ||
                all_buttons[i]->code == YCAPS_MINUS_BTN || all_buttons[i]->code == RADIUS_MINUS_BTN) {
                char text[2] = "-";
                render_text(text, 1, Font, all_buttons[i]->rect, BLACK);
            }
        }


        render_text(param.radius, Font,   radius_value_rect, BLUE);
        render_text(param.x_caps, Font,   xcaps_value_rect,  BLUE);
        render_text(param.y_caps, Font,   ycaps_value_rect,  BLUE);
        render_text(param.scale, 2, Font, scale_value_rect,  BLUE);

        char text[10];
        strncpy(text, "Radius", 6);
        render_text(text, 6, Font, radius_name_rect, DARK_BLUE);
        strncpy(text, "X caps", 6);
        render_text(text, 6, Font, xcaps_name_rect, DARK_BLUE);
        strncpy(text, "Y caps", 6);
        render_text(text, 6, Font, ycaps_name_rect, DARK_BLUE);
        strncpy(text, "Scale", 5);
        render_text(text, 5, Font, scale_name_rect, DARK_BLUE);
        strncpy(text, "Save", 4);
        render_text(text, 4, Font, save_btn.rect, DARK_BLUE);
        strncpy(text, "Source", 6);
        render_text(text, 6, Font, source_description_rect, DARK_BLUE);
        strncpy(text, "Processed", 9);
        render_text(text, 9, Font, result_description_rect, DARK_BLUE);

        if (!saved_changes) render_rectangle(save_btn.rect, DARK_BLUE, 5);

        if (image.Memory) {
            RECT rect_s = compute_rendering_position(source_image_btn.rect, source_image_btn.side, image.Width, image.Height);
            render_bitmap_to_screen(&image, rect_s.left, rect_s.top, rect_s.right - rect_s.left, rect_s.bottom - rect_s.top);
        }

        if (result_bitmap.Memory) {
            RECT rect_r = compute_rendering_position(result_image_btn.rect, result_image_btn.side, result_bitmap.Width, result_bitmap.Height);
            render_bitmap_to_screen(&result_bitmap, rect_r.left, rect_r.top, rect_r.right - rect_r.left, rect_r.bottom - rect_r.top);
        }

        blit_main_buffer_to_window();
    }
}
