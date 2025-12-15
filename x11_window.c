#include <X11/X.h>
#include <X11/Xlib.h>
#include <./stdio.h>
#include <./stdlib.h>
#include <./stdint.h>
#include <./string.h>
#include <stdint.h>
#include <pthread.h>
#include <threads.h>
// #include "fractal.h"

// gcc -o window x11_window.c fractal.c -lX11 -pthread

const int NUMOFTHREADS = 10;

typedef struct {
    int width, height;
    double center_x, center_y;
    double zoom;
    int max_iterations;
} Fractal;

typedef struct{
    void* file;
    Fractal* frac;
    int start, step;
    uint32_t *res;
    unsigned long* colormap;
} PthreadData;

typedef struct {
    Display* dis;
    Window win;
    GC gc;
    XGCValues gcval;
    int screen;
    int width, height;
    XImage *image;
    char* buffer_image;
    uint32_t* pixel_buffer;

    unsigned long* colormap;
    uint8_t **res;
} FWindow;

void* calculate_mandelbrot(PthreadData *data);

void replace_center(Fractal *frac, XEvent e){
    double aspect = (double)frac->width / frac->height;
    frac->center_x = (e.xbutton.x - frac->width / 2.0) * 2.0 / frac->zoom / frac->width + frac->center_x;
    frac->center_y = (e.xbutton.y - frac->height / 2.0) * 2.0 / frac->zoom / aspect / frac->height + frac->center_y;
}

unsigned long* create_colormap(){
    unsigned long *colormap = calloc(256, sizeof(unsigned long));
    if (!colormap) return NULL;
    for (int i = 0; i < 256; i++){
        if (i < 64) colormap[i] = 0 << 16 | 0 << 8 | i * 4;
        else if (i < 128) colormap[i] = 0 << 16 | (i - 64) * 4 << 8 | 255;
        else if (i < 192) colormap[i] = (i - 128) * 4 | 255 << 8 | 255;
        else colormap[i] = (i - 192) * 4 << 16 | 255 << 8 | 255;
    }
    return colormap;
}

uint8_t** realloc_res(XWindowAttributes atr){
    uint8_t **res = calloc(atr.height, sizeof(uint8_t*));
    for (int y = 0; y < atr.height; y++){
        res[y] = calloc(atr.width, sizeof(uint8_t));
    }
    return res;
}

FWindow* Create_win(char* title, int width, int height){
    // standart window
    int count = 0;
    FWindow *win = malloc(sizeof(FWindow));
    if (!win) {
        exit(EXIT_FAILURE);
    }

    win->colormap = create_colormap();

    if (!(win->dis = XOpenDisplay(NULL))){
        free(win);
        fprintf(stderr, "cannot open display\n");
        exit(EXIT_FAILURE);
    }
    printf("%d", count++);
    win->screen = DefaultScreen(win->dis);
    win->width = width;
    win->height = height;
    Window root = RootWindow(win->dis, win->screen);
    win->win = XCreateSimpleWindow(win->dis, root, 0, 0, width, height, 1, BlackPixel(win->dis, win->screen), WhitePixel(win->dis, win->screen));
    XStoreName(win->dis, win->win, title);
    XSelectInput(win->dis, win->win, ExposureMask | KeyPressMask);

    printf("%d", count++);
    // GC
    win->gcval.foreground = BlackPixel(win->dis, win->screen);
    win->gcval.background = WhitePixel(win->dis, win->screen);
    win->gc = XCreateGC(win->dis, win->win, GCBackground | GCForeground, &win->gcval);

    printf("%d", count++);
    // Image
    int depth = DefaultDepth(win->dis, win->screen);
    Visual *vis = DefaultVisual(win->dis, win->screen);
    // buffer create
    size_t buffer_size = width * height * 4;
    if (!(win->buffer_image = malloc(buffer_size))){
        fprintf(stderr, "cannot malloc for buffer");
        free(win);
    }
    memset(win->buffer_image, 0xFF, buffer_size);
    win->image = XCreateImage(win->dis, vis, depth, ZPixmap, 0, win->buffer_image, width, height, 32, width * 4);
    win->pixel_buffer = (uint32_t*)win->buffer_image;

    printf("%d befor show\n", count++);
    // show win
    XMapWindow(win->dis, win->win);
    XFlush(win->dis);

    printf("after show\n");
    // Ждём, пока окно появится
    count = 0;
    XEvent event;
    printf("pupupu\n");
    do {
        printf("%d\n", ++count);
        XNextEvent(win->dis, &event);
        printf("%d\n", ++count);
        if (event.type == KeyPress){
            break;
        }
    } while (1);

    return win;
}

void delete_win(FWindow *win){
    int count = 1;
    printf("startind del %d\n", count++);
    if (!win){
        exit(EXIT_FAILURE);
    }
    printf("%d\n", count++);
    XFreeGC(win->dis, win->gc);
    printf("%d\n", count++);
    XDestroyWindow(win->dis, win->win);
    printf("%d\n", count++);
    XCloseDisplay(win->dis);
    printf("%d\n", count++);
    if (win->buffer_image) free(win->buffer_image);
    printf("%d\n", count++);
    free(win);
    printf("win deleted");
}

int main(){
    double zoom = 1.0;

    Fractal* frac = malloc(sizeof(Fractal));
    frac->zoom = 1.0; frac->center_x = -0.5; frac->center_y = 0.0; frac->max_iterations = 256;
    FWindow *win = Create_win("xuiznaet", 500, 600);

    void* file = fopen("log.txt", "w");

    PthreadData *dates = malloc(NUMOFTHREADS * sizeof(PthreadData));
    pthread_t *threads = malloc(NUMOFTHREADS * sizeof(pthread_t));
    for (int n = 0; n < NUMOFTHREADS; n++){
        dates[n].file = file; dates[n].start = n;
        dates[n].step = NUMOFTHREADS;
        dates[n].frac = frac; dates[n].res = NULL;
        dates[n].colormap = win->colormap;
    }

    XWindowAttributes atr;
    XEvent e;
    while (1){
        XNextEvent(win->dis, &e);
        if (e.type == Expose) {
            fprintf(file, "exposed x: %d, y: %d\n", e.xexpose.width, e.xexpose.height);
        }
        if (e.type == KeyPress){
            if (e.xbutton.button == 9) delete_win(win);
            XGetWindowAttributes(win->dis, win->win, &atr);
            frac->width = atr.width; frac->height = atr.height;
            // win->res = realloc_res(atr);
            win->width = atr.width; win->height = atr.height;
            if (e.xbutton.button == 21) {frac->zoom *= 1.5; printf("%f", frac->zoom);}
            if (e.xbutton.button == 20) frac->zoom -= frac->zoom * 2.0 / 3.0;
            if (e.xbutton.button == 62) replace_center(frac, e);
            fprintf(file, "key %d touched x: %d y: %d \n", e.xbutton.button, atr.width, atr.height);

            // make a buffer
            if (win->buffer_image) { free(win->buffer_image); printf("free\n");}
            if ((win-> buffer_image = malloc(win->width * win->height * 4 * sizeof(char))) != NULL){
                win->pixel_buffer = (uint32_t*)win->buffer_image;}
            printf("expensive: %d size: %zu\n", win->width * win->height * 4, sizeof(win->buffer_image));

            int c = 0;

            for (int n = 0; n < NUMOFTHREADS; n++){
                dates[n].res = win->pixel_buffer;
                pthread_create(&threads[n], NULL, (void*)calculate_mandelbrot, &dates[n]);
                // printf("dtghgnik %d\n", c++);
                // pthread_join(threads[n], NULL);
            }

            for (int n = 0; n < 10; n++){
                pthread_join(threads[n], NULL);
            }
          
            XImage *i = XCreateImage(win->dis, DefaultVisual(win->dis, win->screen), DefaultDepth(win->dis, win->screen), ZPixmap, 0, win->buffer_image, win->width, win->height, 32, win->width * 4);
            XPutImage(win->dis, win->win, win->gc, i, 0, 0, 0, 0, win->width, win->height);
        }
    }
    delete_win(win);
    return 0;
}
// }
