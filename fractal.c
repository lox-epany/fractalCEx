#include <./stdio.h>
#include <./stdlib.h>
#include <./stdint.h>
#include <pthread.h>
// #include <math.h>

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
    unsigned long *colormap;
} PthreadData;

void* calculate_mandelbrot(PthreadData *data) {
    void* file = data->file;
    Fractal *frac = data->frac;

    fprintf(file, "starting calculated\n");

    double scale = 2.0 / frac->zoom;  // Масштаб: 2.0 покрывает диапазон [-1, 1] при zoom=1
    double aspect_ratio = (long double)frac->width / frac->height;  // Соотношение сторон

    double pixel_to_real = scale / frac->width;     // Коэффициент для X координаты
    double pixel_to_imag = scale / aspect_ratio / frac->height;  // Коэффициент для Y координаты

    for (int y = data->start; y < frac->height; y += data->step) {
        for (int x = 0; x < frac->width; x++) {
            double cx = (x - frac->width / 2.0) * pixel_to_real + frac->center_x;
            double cy = (y - frac->height / 2.0) * pixel_to_imag + frac->center_y;

            double z_real = 0.0;
            double z_imag = 0.0;

            int iteration = 0;

            while (z_real * z_real + z_imag * z_imag < 4.0 && iteration < frac->max_iterations) {
                double temp = z_real * z_real - z_imag * z_imag + cx;  // real часть
                z_imag = 2.0 * z_real * z_imag + cy;                   // imag часть
                z_real = temp;

                iteration++;
            }
            data->res[y * frac->width + x] = data->colormap[iteration % 256];
        }
    }
    printf("finished\n");
    pthread_exit(NULL);
    return NULL;
}
