#ifndef IMAGING_OPERATION_H
#define IMAGING_OPERATION_H
#include "../../primitive.h"
#include <himath.h>
#include <stdbool.h>
#include <stdint.h>

typedef union Pixel_ Pixel;
typedef struct Image_ Image;
typedef struct OpInput_ OpInput;

void pixel_clamp(Pixel* pixel, const Image* image);

Image image_load_from_ppm(const char* filename);
Image image_copy(const Image* image);
void image_cleanup(Image* image);
uint image_make_gl_texture(const Image* image);
bool image_is_valid(const Image* image);
Pixel* image_get_pixel_ref(const Image* image, IVec2 tex_coords);
Pixel image_get_pixel_val(const Image* image, IVec2 tex_coords);
void image_set_pixel(const Image* image, IVec2 tex_coords, Pixel value);
Pixel image_sample_nearest(const Image* image, FVec2 uv);

void image_add(Image* image, const Image* other);
void image_sub(Image* image, const Image* other);
void image_product(Image* image, const Image* other);
void image_negate(Image* image);
void image_log(Image* image, float constant, float base);
void image_power(Image* image, float constant, float gamma);
void image_ccl(Image* image,
               uint16_t neighbor_bits,
               int background_max_intensity);
void image_equalize_histogram(Image* image);
void image_match_histogram(Image* image, const Image* ref);
void image_gaussian_smoothen();

#define ON_FINISH_OP(name)                                                     \
    void name(const Image* result_image, int op_index, void* udata)
typedef ON_FINISH_OP(OnFinishOpFn);

void image_process_operations(Image* original,
                              const OpInput* inputs,
                              int inputs_count,
                              OnFinishOpFn* on_finish_op_callback,
                              void* udata);
void image_save_to_file(const Image* image);

#endif // IMAGING_OPERATION_H