#include <windows.h>
#include <string>
using namespace std;

/* Todo list:
    - handle 1 and 3 channels image inputs
    - investigate loading all caps
    - save image
    
    - make everyting settable
    - make UI
*/




#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// #define assert(Expression) if(!(Expression)) *(int *) 0 = 0;

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

uint32 key_presses[100]    = {0};
uint32 key_releases[100]   = {0};
int8   key_presses_length  = 0;
int8   key_releases_length = 0;
bool   left_button_down    = false;
bool   right_button_down   = false;

bool changed_size = false;

#define Total_Caps 99 // Don't know why only 99 load
bitmap caps_data[Total_Caps];

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

void reset_window_background() {
    HDC DeviceContext = GetDC(Window);

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    int w_width  = ClientRect.right  - ClientRect.left;
    int w_height = ClientRect.bottom - ClientRect.top;


    StretchDIBits(DeviceContext,
                  0, 0, w_width, w_height,
                  0, 0, w_width, w_height,
                  0, 0, DIB_RGB_COLORS, BLACKNESS);
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
        case WM_LBUTTONDOWN: left_button_down  = true;  break;
        case WM_LBUTTONUP:   left_button_down  = false; break;
        case WM_RBUTTONDOWN: right_button_down = true;  break;
        case WM_RBUTTONUP:   right_button_down = false; break;
        default:
            Result = DefWindowProc(Window, Message, WParam, LParam);
            break;
    }

    return Result;
}


#define INITIAL_WIDTH  800
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

struct Color {
    int R;
    int G;
    int B;
    int A;
};

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

void render_bitmap(bitmap *Dest, bitmap *Source, int x, int y, int width, int height) {

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

            // uint8 A = ((source_pixel & 0xff000000) >> 24);
            // uint8 R = ((source_pixel & 0x00ff0000) >> 16);
            // uint8 G = ((source_pixel & 0x0000ff00) >>  8);
            // uint8 B = ((source_pixel & 0x000000ff) >>  0);

            float SA = ((source_pixel & 0xff000000) >> 24) / 255.0;
            uint8 SR = ((source_pixel & 0x00ff0000) >> 16);
            uint8 SG = ((source_pixel & 0x0000ff00) >>  8);
            uint8 SB = ((source_pixel & 0x000000ff) >>  0);

            float DA = ((*Pixel & 0xff000000) >> 24) / 255.0;
            uint8 DR = ((*Pixel & 0x00ff0000) >> 16);
            uint8 DG = ((*Pixel & 0x0000ff00) >>  8);
            uint8 DB = ((*Pixel & 0x000000ff) >>  0);

            uint8 A = 255 * (SA + DA - SA*DA);
            uint8 R = DR * (1 - SA) + SR;
            uint8 G = DG * (1 - SA) + SG;
            uint8 B = DB * (1 - SA) + SB;

            *Pixel = (A << 24) | (R << 16) | (G << 8) | B;
            // printf("%d %d %d %d\n", A, R, G, B);
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

    return (float) sum / (float) count;
}

int clamp(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

int *compute_indexes(bitmap image, v2 *centers, int number_of_centers, int radius) {

    int *indexes = (int *) malloc(sizeof(int) * number_of_centers);

    int min_index = Total_Caps;
    int max_index = 0;

    for (int i = 0; i < number_of_centers; i++) {
        auto gray_value = compute_gray_value(image, centers[i].x, centers[i].y, radius);
        indexes[i] = (gray_value / 255.0) * Total_Caps;

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

void compute_caps_dimensions(bitmap image, int radius, int *horizontal_caps, int *vertical_caps) {
    int bmp_w = image.Width;
    int bmp_h = image.Height;

    *horizontal_caps = 0;
    *vertical_caps   = 0;

    int delta_horizontal = 2 * radius;
    int current_x = radius;
    while (current_x + radius < bmp_w) {
        current_x += delta_horizontal;
        (*horizontal_caps) = *horizontal_caps + 1;
    }

    int delta_vertical = (SQRT_2 * radius) + 0.5;
    int current_y = radius;
    while (current_y + radius < bmp_h) {
        current_y += delta_vertical;
        (*vertical_caps) = *vertical_caps + 1;
    }
}

void premultiply_alpha(bitmap *image) {
    uint32 *Pixels = (uint32 *) image->Memory;
    for (int p = 0; p < image->Width * image->Height; p++) {
        uint8 A = ((Pixels[p] & 0xff000000) >> 24);
        float A_scaled = (float) A / 255.0;
        uint8 R = ((Pixels[p] & 0x00ff0000) >> 16) * A_scaled;
        uint8 G = ((Pixels[p] & 0x0000ff00) >>  8) * A_scaled;
        uint8 B = ((Pixels[p] & 0x000000ff) >>  0) * A_scaled;
        
        Pixels[p] = (A << 24) | (R << 16) | (G << 8) | (B << 0);
    }
}

// #define DEBUG

int main(void) {
    initialize_main_buffer();
    start_main_window();
    
    reset_window_background();

    int max_cap_width  = 0;
    int max_cap_height = 0;

    int Bpp;
    for (int i = 0; i < Total_Caps; i++) {
        const int8 * filepath = ("Caps/Cap " + to_string(i + 1) + ".png\0").c_str();
        caps_data[i].Memory = stbi_load(filepath, &caps_data[i].Width, &caps_data[i].Height, &Bpp, 0);
        if (caps_data[i].Memory == NULL) printf("ERROR\n");
        assert(Bpp == Bytes_Per_Pixel);

        premultiply_alpha(&caps_data[i]);

        // printf("Loaded %s w: %d h: %d\n", to_string(i+1).c_str(), caps_data[i].Width, caps_data[i].Height);
        if (caps_data[i].Width  > max_cap_width)  max_cap_width  = caps_data[i].Width;
        if (caps_data[i].Height > max_cap_height) max_cap_height = caps_data[i].Height;
    }

    bitmap image;
    image.Memory = stbi_load("Caps/Cap 1.png", &image.Width, &image.Height, &Bpp, 0);
    assert(Bpp == Bytes_Per_Pixel);
    premultiply_alpha(&image);

    int x_caps = 0;
    int y_caps = 0;
    int radius = image.Width / (40 * 2);
    compute_caps_dimensions(image, radius, &x_caps, &y_caps);

    v2 *centers = (v2 *) malloc(sizeof(v2) * x_caps * y_caps);
    compute_centers(centers, x_caps, y_caps, radius);

    int *indexes = compute_indexes(image, centers, x_caps * y_caps, radius);

    #if defined(DEBUG)
    printf("max cap w: %d max cap h: %d\nimage width: %d image height: %d\ncaps per row: %d caps per col: %d radius: %d\n", max_cap_width, max_cap_height, image.Width, image.Height, x_caps, y_caps, radius);
    for (int i = 0; i < x_caps * y_caps; i++) printf("[%d] x: %d y: %d, index = %d\n", i, centers[i].x, centers[i].y, indexes[i]);
    #endif

    bitmap composited_bitmap;

    int scale = 4;
    radius *= scale;
    for (int i = 0; i < x_caps * y_caps; i++) {
        centers[i].x *= scale;
        centers[i].y *= scale;
    }


    memset(Main_Buffer.Memory, 0, Main_Buffer.Width * Main_Buffer.Height * Bytes_Per_Pixel);
    for (int i = 0; i < x_caps * y_caps; i++) {
        render_bitmap(&Main_Buffer, &caps_data[indexes[i]], centers[i].x - radius, centers[i].y - radius, radius * 2, radius * 2);
    }

    while (true) {
        auto result = handle_window_messages();
        if (result == -1) return 0;

        // render_rectangle({50, 50, 450, 700}, {200, 200, 200, 200}, 4);
        blit_main_buffer_to_window();
        // return 0;
    }
}
