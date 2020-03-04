#ifndef IMAGING_MODEL_H
#define IMAGING_MODEL_H
#include "../../all.h"

typedef union Pixel_
{
    struct
    {
        int32_t r;
        int32_t g;
        int32_t b;
        int32_t a;
    };

    int32_t e[4];
} Pixel;

typedef struct Image_
{
    IVec2 size;
    int32_t max_color;
    Pixel* pixels;
} Image;

typedef struct ImageKeyValue_
{
    histr_String key;
    Image value;
} ImageKeyValue;

typedef enum OpType_
{
    OpType_Undefined = 0,
    OpType_Add,
    OpType_Sub,
    OpType_Product,
    OpType_Negate,
    OpType_Log,
    OpType_Power,
    OpType_CCL,
    OpType_EqualizeHistogram,
    OpType_MatchHistogram,
    OpType_GaussianSmoothen,
    OpType_Count,
} OpType;

typedef struct OpInputAdd_
{
    const ImageKeyValue* ref_image_kv;
} OpInputAdd;

typedef OpInputAdd OpInputSub;
typedef OpInputAdd OpInputProduct;

typedef struct OpInputLog_
{
    float constant;
    float base;
} OpInputLog;

typedef struct OpInputPower_
{
    float constant;
    float gamma;
} OpInputPower;

typedef struct OpInputCCL_
{
    uint16_t neighbor_bits;
    int background_max_intensity;
} OpInputCCL;

typedef struct OpInput_
{
    OpType type;
    union
    {
        OpInputAdd add;
        OpInputSub sub;
        OpInputProduct product;
        OpInputLog log;
        OpInputPower power;
        OpInputCCL ccl;
    };
} OpInput;

#endif // IMAGING_MODEL_H