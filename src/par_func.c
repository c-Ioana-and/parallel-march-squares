#include "helpers.h"
#include "par_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


// Function that initializes the grid used for sampling the image 
unsigned char** init_grid (int p, int q) {
    unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char *));

    for (int i = 0; i <= p; i++) {
        grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }

    if (!grid) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    return grid;
}


// Corresponds to step 1 of the marching squares algorithm, which focuses on sampling the image.
// Builds a p x q grid of points with values which can be either 0 or 1, depending on how the
// pixel values compare to the `sigma` reference value. The points are taken at equal distances
// in the original image, based on the `step_x` and `step_y` arguments.
unsigned char **sample_grid(unsigned char** grid, ppm_image *image, int step_x, int step_y, unsigned char sigma,
                                        int thread_id, int nr_threads) {
    int p = image->x / step_x;
    int q = image->y / step_y;

    // the matrix is split into nr_threads parts, each thread working on a part
    int start1 = thread_id * p / nr_threads;
    int end1 = (thread_id + 1) * p / nr_threads;

    for (int i = start1; i < end1; i++) {
        for (int j = 0; j < q; j++) {
            ppm_pixel curr_pixel = image->data[i * step_x * image->y + j * step_y];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > sigma) {
                grid[i][j] = 0;
            } else {
                grid[i][j] = 1;
            }
        }
    }
    grid[p][q] = 0;

    // last sample points have no neighbors below / to the right, so we use pixels on the
    // last row / column of the input image for them
    for (int i = start1; i < end1; i++) {
        ppm_pixel curr_pixel = image->data[i * step_x * image->y + image->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            grid[i][q] = 0;
        } else {
            grid[i][q] = 1;
        }
    }
    for (int j = start1; j < end1; j++) {
        ppm_pixel curr_pixel = image->data[(image->x - 1) * image->y + j * step_y];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > sigma) {
            grid[p][j] = 0;
        } else {
            grid[p][j] = 1;
        }
    }

    return grid;
}

// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.
void update_image(ppm_image *image, ppm_image *contour, int x, int y) {
    for (int i = 0; i < contour->x; i++) {
        for (int j = 0; j < contour->y; j++) {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
}

// Corresponds to step 2 of the marching squares algorithm, which focuses on identifying the
// type of contour which corresponds to each subgrid. It determines the binary value of each
// sample fragment of the original image and replaces the pixels in the original image with
// the pixels of the corresponding contour image accordingly.
void march(ppm_image *image, unsigned char **grid, ppm_image **contour_map, int step_x, int step_y,
                                                    int thread_id, int nr_threads) {
    int p = image->x / step_x;
    int q = image->y / step_y;

    // the matrix is split into nr_threads parts, each thread working on a part
    int start = thread_id * p / nr_threads;
    int end = (thread_id + 1) * p / nr_threads;

    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k = 8 * grid[i][j] + 4 * grid[i][j + 1] + 2 * grid[i + 1][j + 1] + 1 * grid[i + 1][j];
            update_image(image, contour_map[k], i * step_x, j * step_y);
        }
    }
}


// Function used to initialize the rescaled image
ppm_image *init_image(ppm_image *image) {
    // we only rescale downwards
    if (image->x <= RESCALE_X && image->y <= RESCALE_Y) {
        return image;
    }

    // alloc memory for image
    ppm_image *new_image = (ppm_image *)malloc(sizeof(ppm_image));
    if (!new_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    new_image->x = RESCALE_X;
    new_image->y = RESCALE_Y;

    new_image->data = (ppm_pixel*)malloc(new_image->x * new_image->y * sizeof(ppm_pixel));
    if (!new_image) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    return new_image;
}

// Function used to rescale the image, parallelized
void rescale_image(ppm_image *image, ppm_image *new_image, int thread_id, int nr_threads) {
    if (new_image == image) {
        return;
    }
    uint8_t sample[3];

    // the matrix is split into nr_threads parts, each thread working on a part
    int start = thread_id * new_image->x / nr_threads;
    int end = (thread_id + 1) * new_image->x / nr_threads;

    // use bicubic interpolation for scaling
    for (int i = start; i < end; i++) {
        for (int j = 0; j < new_image->y; j++) {
            float u = (float)i / (float)(new_image->x - 1);
            float v = (float)j / (float)(new_image->y - 1);
            sample_bicubic(image, u, v, sample);

            new_image->data[i * new_image->y + j].red = sample[0];
            new_image->data[i * new_image->y + j].green = sample[1];
            new_image->data[i * new_image->y + j].blue = sample[2];
        }
    }
}


// Function executed by each thread : it basically executes the origninal
// rescale_image, march and sample_grid functions, but parallelized
void *thread_business(void *args) {
    struct thread_args *arg = (struct thread_args *)args;

    ppm_image *image = arg->image;
    ppm_image *new_image = arg->new_image;
    unsigned char **grid = arg->grid;
    ppm_image **contour_map = arg->contour_map;
    int thread_id = arg->thread_id;
    int nr_threads = arg->nr_threads;
    pthread_barrier_t *barrier = arg->barrier;

    // rescale image, then wait for all threads to finish rescaling
    if (new_image != image) {
        rescale_image(image, new_image, thread_id, nr_threads);
        pthread_barrier_wait(barrier);
    }

    // sample grid, then wait for all threads to finish sampling
    sample_grid(grid, new_image, STEP, STEP, SIGMA, thread_id, nr_threads);
    pthread_barrier_wait(barrier);
    
    march(new_image, grid, contour_map, STEP, STEP, thread_id, nr_threads);
    pthread_barrier_wait(barrier);
    return NULL;
}

