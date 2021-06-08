#include <SDL2/SDL.h>

#define WINDOW_SIZE 800
#define MAX_ITERATIONS 400
#define NUM_THREADS 4
/* Macro to scale coords with given coord and dimension */
#define SCALE_COORD(coord, dimension) (range->coord + (range->dimension * (double)(coord) / WINDOW_SIZE))

/* Struct to keep fractal range as doubles, not float like in SDL_FRect */
typedef struct FractalRange { double x; double y; double w; double h; } FractalRange;
/* Context for thread */
typedef struct WorkerFractalContext {
    int rowStart;   /* What row thread must start with drawing */
    int rowEnd;     /* What row thread must end with drawing (excluded) */

    unsigned int (*fn)(const double*, const double*); /* pointer to which function is drawing */
    SDL_Surface* fractal;   /* To avoid global variables I have to keep in every context a pointer to drawing surface */
    FractalRange* range;    /* Same as above, but now I need to keep a pointer to fractal range */
    SDL_sem* semaphore;     /* Semaphore for every thread, telling him whenever it must draw or not */
    int* isRunning;         /* A pointer to global variable of is program still running */
} WorkerFractalContext;

/* The function which calculates the given pixel with Mandelbrot expression */
unsigned int countOneMandelbrot(const double* x, const double* y) {
    double zx = *x; /* Copy given x */
    double zy = *y; /* Copy given y */
    double temp = 0; /* Temporary variable to store the new zx value */
    unsigned int iterations = 0;    /* The amount of iterations used for find looping */

    /* If you don't know what happend next, you need to read some Wikipedia, idk */
    while (iterations < MAX_ITERATIONS && zx * zx + zy * zy < 4.0) {
        temp = zx * zx - zy * zy + *x;
        zy = 2.0 * zx * zy + *y;
        zx = temp;
        iterations++;
    }
    return iterations;
}
/* I must say, that those two underlying functions much same as the function above, the only difference in the calculation method */
unsigned int countOneBurningShip(const double* x, const double* y) {
    double zx = *x;
    double zy = *y;
    double temp = 0;
    unsigned int iterations = 0;

    while (iterations < MAX_ITERATIONS && zx * zx + zy * zy < 4.0) {
        temp = zx * zx - zy * zy + *x;
        zy = SDL_fabs(2.0 * zx * zy) + *y;
        zx = temp;
        iterations++;
    }
    return iterations;
}
unsigned int countOneTricorn(const double* x, const double* y) {
    double zx = *x;
    double zy = *y;
    double temp = 0;
    unsigned int iterations = 0;

    while (iterations < MAX_ITERATIONS && zx * zx + zy * zy < 4.0) {
        temp = zx * zx - zy * zy + *x;
        zy = -2.0 * zx * zy + *y;
        zx = temp;
        iterations++;
    }
    return iterations;
}

/* Well, complicated thing; this function is the thread function that runs in the background and gets the appropriate context */
int oneRowThread(void* ptr) {
    WorkerFractalContext* workerContext = (WorkerFractalContext*)ptr; /* Here we cast the ptr to thread context */
    FractalRange* range = workerContext->range;         /* This is the range of fractal */
    Uint32* pixels = workerContext->fractal->pixels;    /* This is the actual pixels of fractal surface */
    SDL_PixelFormat* format = workerContext->fractal->format; /* This is the pixel format; need for convert three integers into RGB integer */
    int* isRunning = workerContext->isRunning;  /* This is the simplified pointer to variable of running the app or not */

    double xCoord = 0;  /* The variable of scaled x */
    double yCoord = 0;  /* The variable of scaled y *

    /* Those four variables is using for convert HSB into RGB */
    double H = 0;
    double r = 0;
    double g = 0;
    double b = 0;

    int iterations = 0; /* Amount of iterations each pixel have elapsed */
    Uint32 color = 0;   /* Color for each pixel */
    while ((*isRunning)) { /* While app is still running */
        SDL_SemWait(workerContext->semaphore);  /* Try to get the semaphore; if it is 0 - pause the thread */

        /* Iterate over every planned row until the end or before reach WINDOW_SIZE */
        for (int y = workerContext->rowStart; y < WINDOW_SIZE && y < workerContext->rowEnd; y++) {
            yCoord = SCALE_COORD(y, h); /* Scale every y */
            for (int x = 0; x < WINDOW_SIZE; x++) {
                xCoord = SCALE_COORD(x, w); /* Scale every x */
                iterations = workerContext->fn(&xCoord, &yCoord); /* Run count function from context */
                color = 0;  /* By default, set the color to black */
                if (iterations != MAX_ITERATIONS) { /* If iterations is not max, convert to color then */
                    H = 7.0 + iterations / 200.0;
                    /** HSB to RGB convertion; simplified due to S and V = 100.0 **/
                    r = 10000.0 * SDL_cos(H) - 9900;
                    g = 10000.0 * SDL_cos(H - 2.09439) - 9900;
                    b = 10000.0 * SDL_cos(H + 2.09439) - 9900;
                    color = SDL_MapRGB(format, (Uint8)r, (Uint8)g, (Uint8)b);
                    /***************************/
                }
                pixels[y * WINDOW_SIZE + x] = color; /* Set the calculated color to current pixel; here we pretend that 1D array is 2D */
            }
        }
    }
}
/** We need to scale x coords to fractal coords */
void zoomFractal(Sint32 x, Sint32 y, FractalRange* range, double scale) {
    /* zoom in or out; scale matters */
    range->x = SCALE_COORD(x, w) - range->w * scale * 0.5;
    range->y = SCALE_COORD(y, h) - range->h * scale * 0.5;
    /* increase width and height of current range */
    range->w *= scale;
    range->h *= scale;
}

/* Pass the pointer for drawing function to change the drawing fractal and the contexts */
void drawOtherFractal(WorkerFractalContext* contexts, unsigned int (*fn)(const double*, const double*)) {
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].fn = fn;    /* Set the new drawing function to every thread; again, no global vars */
        SDL_SemPost(contexts[i].semaphore); /* Telling every thread to draw */
    }
}
/* Pass to function contexes for every thread */
void drawSameFractal(WorkerFractalContext* contexts) {
    for (int i = 0; i < NUM_THREADS; i++)
        SDL_SemPost(contexts[i].semaphore); /* Telling every thread to draw */
}

/* Main function for SDL app */
int SDL_main(int argv, char** args) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) /* Init needed things from SDL */
        return 1;

    /* Create new SDL window */
    SDL_Window* window = SDL_CreateWindow("Fractals", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_SIZE, WINDOW_SIZE, SDL_WINDOW_SHOWN);
    if (!window)
        return 1;

    int isRunning = 1; /* Variable for determine is app still running */
    FractalRange range = { -2.0, -1.5, 3.0, 3.0 };  /* The range of fractal */
    SDL_Surface* fractal = SDL_GetWindowSurface(window);    /* Surface of fractal where to draw */
    SDL_Thread** threads = SDL_malloc(sizeof(SDL_Thread*) * NUM_THREADS);   /* Threads pool */
    WorkerFractalContext* contexts = SDL_malloc(sizeof(WorkerFractalContext) * NUM_THREADS); /* Contexes array */
    for (int i = 0; i < NUM_THREADS; i++) { /* For every thread */
        /* Set the values for every context */
        contexts[i].rowStart = i * WINDOW_SIZE / NUM_THREADS;
        contexts[i].rowEnd = (i + 1) * WINDOW_SIZE / NUM_THREADS;
        contexts[i].range = &range; /* Pass the pointer for range */
        contexts[i].isRunning = &isRunning; /* Pass the pointer for isRunning variable */
        contexts[i].semaphore = SDL_CreateSemaphore(1); /* Set the new semaphore value 1 for perfom first draw */
        contexts[i].fractal = fractal;  /* Passing the variable for keep the fractal surface */
        contexts[i].fn = countOneMandelbrot;    /* Presume that at start user want to see a Mandelbrot set */
        threads[i] = SDL_CreateThread(oneRowThread, "Draw Thread", (void*)&contexts[i]); /* Create new thread and pass created context */
    }

    SDL_Event event; /* The variable to keep the events */
    while (isRunning) {    /* While app is running */
        while (SDL_PollEvent(&event)) {  /* Get the event */
            switch (event.type) {
            case SDL_QUIT:  /* When press the cross for exit */
                isRunning = 0;  /* Well, it's not running anymore */
                break;
            case SDL_KEYDOWN:   /* Handle the numbers on keyboard */
                switch (event.key.keysym.sym) {
                case SDLK_1:    /* Number 1 will render the Mandelbrot set */
                    range.x = -2.0; range.y = -1.5;
                    range.w = 3.0;  range.h = 3.0;
                    drawOtherFractal(contexts, countOneMandelbrot);
                    break;
                case SDLK_2:    /* Number 2 will render the Burning Ship fractal */
                    range.x = -2.0; range.y = -2.5;
                    range.w = 3.0;  range.h = 4.0;
                    drawOtherFractal(contexts, countOneBurningShip);
                    break;
                case SDLK_3:    /* Number 3 will render the Tricorn fractal */
                    range.x = -2.0; range.y = -2.5;
                    range.w = 4.0;  range.h = 4.0;
                    drawOtherFractal(contexts, countOneTricorn);
                    break;
                case SDLK_ESCAPE:   /* When press escape button we want to close the app */
                    isRunning = 0;
                    break;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:   /* When mouse click */
                switch (event.button.button) {
                case SDL_BUTTON_LEFT:   /* When clicked left mouse button, then zoom in */
                    zoomFractal(event.button.x, event.button.y, &range, 0.5);
                    drawSameFractal(contexts);  /* Redraw the same fractal, but with different fractal range */
                    break;
                case SDL_BUTTON_RIGHT:  /* When clicker right mouse button, then zoom out */
                    zoomFractal(event.button.x, event.button.y, &range, 2);
                    drawSameFractal(contexts);  /* Redraw the same fractal, but with different fractal range */
                    break;
                }
            }
        }
        SDL_UpdateWindowSurface(window);    /* Update the window surface */
    }

    for (int i = 0; i < NUM_THREADS; i++)
        SDL_DestroySemaphore(contexts[i].semaphore);    /* Destroy every created semaphore */
    SDL_free(contexts); /* Clear the context array */
    SDL_free(threads);  /* Clear the thread pool */
    SDL_DestroyWindow(window); /* Delete window from heap */
    SDL_Quit(); /* Clear all SDL things, I guess */

    return 0;
}