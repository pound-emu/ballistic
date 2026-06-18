#ifndef CIMGUIZMO_INCLUDED
#define CIMGUIZMO_INCLUDED

#include "cimgui.h"

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS

typedef int vgButtons;
typedef int vgModifiers;

typedef struct Vec4
{
    float x, y, z, w;
} Vec4;

typedef struct vec4
{
    float x, y, z, w;
} vec4;

typedef struct vec3
{
    float x, y, z;
} vec3;

typedef struct Mat4
{
    union
    {
        float f[16];
        Vec4  v[4];
        /*struct {      float m00, m01, m02, m03,
                        m10, m11, m12, m13,
                        m20, m21, m22, m23,
                        m30, m31, m32, m33; };*/
    };
} Mat4;

typedef struct quat
{
    float x, y, z, w;
} quat;

#include "imgui_structs.h"
#else
#endif // CIMGUI_DEFINE_ENUMS_AND_STRUCTS

PLACE_STRUCTS_C

#include "auto_funcs.h"

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
CIMGUI_API void mat4_cast(quat *q, Mat4 *mat);
CIMGUI_API void mat4_pos_cast(quat *q, vec3 pos, Mat4 *mat);
CIMGUI_API void quat_cast(float f[16], quat *qq);
CIMGUI_API void quat_pos_cast(float f[16], quat *qq, vec3 *pos);
#else
CIMGUI_API void mat4_cast(quat *q, Mat4 *mat);
CIMGUI_API void mat4_pos_cast(quat *q, vec3 pos, Mat4 *mat);
CIMGUI_API void quat_cast(float f[16], quat *qq);
CIMGUI_API void quat_pos_cast(float f[16], quat *qq, vec3 *pos);
#endif

#endif // CIMGUIZMO_INCLUDED
