#include <windows.h>
#include <string>
using namespace std;

/* Todo list:
    - save image
    
    - make everyting settable
    - make UI
*/

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
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

uint32 key_presses[100]  = {0};
uint32 key_releases[100] = {0};
int8 key_presses_length  = 0;
int8 key_releases_length = 0;
bool left_button_down    = false;
bool right_button_down   = false;

bool changed_size = false;

#define Total_Caps 121 // Don't know why only 99 load
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


#define INITIAL_WIDTH  1200
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

struct conversion_parameters {
    int x_caps, y_caps;
    int radius;
    float scale;
};

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
    result_bitmap.Width  = blit_radius * (2 * param.x_caps + 1);
    result_bitmap.Height = blit_radius * (SQRT_2 * param.y_caps);
    result_bitmap.Memory = (uint8 *) malloc(result_bitmap.Width * result_bitmap.Height * Bytes_Per_Pixel);

    // TODO: look into having a black background
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

    if (!left_button_down) return -1;

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

int main(void) {
    initialize_main_buffer();
    start_main_window();
    
    reset_window_background();

    int max_cap_width  = 0;
    int max_cap_height = 0;

    int Bpp;
    for (int i = 0; i < Total_Caps; i++) {
        string filepath = "Caps/Cap " + to_string(i + 1) + ".png";
        caps_data[i].Memory = stbi_load(filepath.c_str(), &caps_data[i].Width, &caps_data[i].Height, &Bpp, Bytes_Per_Pixel);
        if (caps_data[i].Memory == NULL) printf("ERROR loading file: %s\n", filepath.c_str());
        assert(Bpp == Bytes_Per_Pixel);

        premultiply_alpha(&caps_data[i]);

        // printf("Loaded %s w: %d h: %d\n", to_string(i+1).c_str(), caps_data[i].Width, caps_data[i].Height);
        if (caps_data[i].Width  > max_cap_width)  max_cap_width  = caps_data[i].Width;
        if (caps_data[i].Height > max_cap_height) max_cap_height = caps_data[i].Height;
    }

    string filepath = "Thomas Brodie Sangster.jpg";

    bitmap image;
    image.Memory = stbi_load(filepath.c_str(), &image.Width, &image.Height, &Bpp, 0);
    adjust_bpp(&image, Bpp);
    premultiply_alpha(&image);

    conversion_parameters param;

    param.radius = 4;
    compute_caps_dimensions(image, param.radius, &param.x_caps, &param.y_caps);
    param.scale = 1;

    bitmap result_bitmap = create_image(image, param);

    button *all_buttons[100];
    int btn_length = 0;

    button source_image_btn, result_image_btn;
    button save_btn;

    source_image_btn.rect = {50, 50, 450, 750};
    source_image_btn.side = 5;
    source_image_btn.c    = {140, 140, 140, 255};
    source_image_btn.code = 1;
    
    result_image_btn.rect = {700, 50, 1100, 750};
    result_image_btn.side = 5;
    result_image_btn.c    = {140, 140, 140, 255};
    result_image_btn.code = 2;
    
    save_btn.rect = {500, 300, 650, 350};
    save_btn.side = 100;
    save_btn.c    = {50, 180, 50, 255};
    save_btn.code = 3;

    all_buttons[btn_length++] = &source_image_btn;
    all_buttons[btn_length++] = &result_image_btn;
    all_buttons[btn_length++] = &save_btn;
    
    while (true) {
        auto result = handle_window_messages();
        if (result == -1) return 0;

        for (int i = 0; i < btn_length; i++) {
            render_rectangle(all_buttons[i]->rect, all_buttons[i]->c, all_buttons[i]->side);
        }

        int pressed = button_pressed(all_buttons, btn_length);
        switch (pressed) {
            case -1: break;
            case 3:
                int success = stbi_write_png("result.png", result_bitmap.Width, result_bitmap.Height, Bytes_Per_Pixel, result_bitmap.Memory, Bytes_Per_Pixel * result_bitmap.Width);
                if (success == 0) printf("Failed to write image to file!!\n");
                break;
        }

        RECT rect_s = compute_rendering_position(source_image_btn.rect, source_image_btn.side, image.Width, image.Height);
        render_bitmap_to_screen(&image, rect_s.left, rect_s.top, rect_s.right - rect_s.left, rect_s.bottom - rect_s.top);

        RECT rect_r = compute_rendering_position(result_image_btn.rect, result_image_btn.side, result_bitmap.Width, result_bitmap.Height);
        render_bitmap_to_screen(&result_bitmap, rect_r.left, rect_r.top, rect_r.right - rect_r.left, rect_r.bottom - rect_r.top);

        blit_main_buffer_to_window();
    }
}
