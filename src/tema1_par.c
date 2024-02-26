// Author: APD team, except where source was noted

#include "helpers.h"
#include "par_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define CONTOUR_CONFIG_COUNT    16
#define FILENAME_MAX_SIZE       50
#define STEP                    8
#define SIGMA                   200
#define RESCALE_X               2048
#define RESCALE_Y               2048

#define CLAMP(v, min, max) if(v < min) { v = min; } else if(v > max) { v = max; }

// Creates a map between the binary configuration (e.g. 0110_2) and the corresponding pixels
// that need to be set on the output image. An array is used for this map since the keys are
// binary numbers in 0-15. Contour images are located in the './contours' directory.
ppm_image **init_contour_map() {
    ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!map) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        map[i] = read_ppm(filename);
    }

    return map;
}



// Calls `free` method on the utilized resources.
void free_resources(ppm_image *image, ppm_image **contour_map, unsigned char **grid, int step_x) {
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(contour_map[i]->data);
        free(contour_map[i]);
    }
    free(contour_map);

    for (int i = 0; i <= image->x / step_x; i++) {
        free(grid[i]);
    }
    free(grid);

    free(image->data);
    free(image);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }

    ppm_image *image = read_ppm(argv[1]);
    int step_x = STEP;
    int step_y = STEP;

    // 0. Initialize contour map
    ppm_image **contour_map = init_contour_map();

    // 1. Initialize the rescaled image
    ppm_image *scaled_image = init_image(image);

    // 2. Intialize the grid
    int p = scaled_image->x / step_x;
    int q = scaled_image->y / step_y;
    unsigned char **grid = init_grid(p, q);

    // 3. Initialize the threads used in parallelization
    // for the functions rescale_image, march, sample_grid
    char nr_threads = atoi(argv[3]);
    pthread_t threads[nr_threads];
   
    // each thread will receive a struct thread_args
    struct thread_args *args[nr_threads];

    // initialize barrier
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, nr_threads);

    for (int i = 0; i < nr_threads; i++) {
        args[i] = (struct thread_args *)malloc(sizeof(struct thread_args));
        if (!args[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
        args[i]->image = image;
        args[i]->new_image = scaled_image;
        args[i]->contour_map = contour_map;
        args[i]->grid = grid;
        args[i]->thread_id = i;
        args[i]->nr_threads = nr_threads;
        args[i]->barrier = &barrier;
        
        pthread_create(&threads[i], NULL, thread_business, args[i]);
    }
    
    // wait for threads to finish
    for (int i = 0; i < nr_threads; i++) {
        pthread_join(threads[i], NULL);
        free(args[i]);
    }

    // 4. Write output
    write_ppm(scaled_image, argv[2]);

    free_resources(scaled_image, contour_map, grid, step_x);

    return 0;
}
