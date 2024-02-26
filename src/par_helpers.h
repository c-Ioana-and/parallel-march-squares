#ifndef PAR_HELPERS_H
#define PAR_HELPERS_H

struct thread_args {
    ppm_image *image;
    unsigned char **grid;
    ppm_image **contour_map;
    ppm_image *new_image;
    int thread_id;
    int nr_threads;
    pthread_barrier_t *barrier;
};

unsigned char** init_grid (int, int);
void *thread_business(void *);
unsigned char **sample_grid(unsigned char**, ppm_image *, int, int, unsigned char, int, int);
void march(ppm_image *, unsigned char **, ppm_image **, int, int, int, int);
void update_image(ppm_image *, ppm_image *, int, int);
void rescale_image(ppm_image *, ppm_image *, int, int);
ppm_image *init_image(ppm_image *);

#endif