#include "imgui.h"
#include "imgui_internal.h"
#include "imGuIZMOquat.h"
#include "cimguizmo_quat.h"



#include "auto_funcs.cpp"

CIMGUI_API void mat4_cast( quat *q,Mat4* mat)
{
	*mat = mat4_cast(*q);
}

CIMGUI_API void mat4_pos_cast( quat *q, vec3 pos, Mat4* mat)
{
	*mat = mat4_cast(*q);
	mat->m30 = pos.x;
	mat->m31 = pos.y;
	mat->m32 = pos.z;
}

struct m16 {
    union {
        float v[16];
        struct {      float m00, m10, m20, m30,
                        m01, m11, m21, m31,
                        m02, m12, m22, m32,
                        m03, m13, m23, m33; };
    };
};

//https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
CIMGUI_API void quat_cast(float f[16], quat *qq)
{
	m16 m = *(m16*)f;
	float qw, qx, qy, qz;
	float tr = m.m00 + m.m11 + m.m22;

	if (tr > 0) { 
	float S = sqrt(tr+1.0f) * 2.0f; // S=4*qw 
	qw = 0.25f * S;
	qx = (m.m21 - m.m12) / S;
	qy = (m.m02 - m.m20) / S; 
	qz = (m.m10 - m.m01) / S; 
	} else if ((m.m00 > m.m11)&&(m.m00 > m.m22)) { 
	float S = sqrt(1.0f + m.m00 - m.m11 - m.m22) * 2.0f; // S=4*qx 
	qw = (m.m21 - m.m12) / S;
	qx = 0.25f * S;
	qy = (m.m01 + m.m10) / S; 
	qz = (m.m02 + m.m20) / S; 
	} else if (m.m11 > m.m22) { 
	float S = sqrt(1.0f + m.m11 - m.m00 - m.m22) * 2.0f; // S=4*qy
	qw = (m.m02 - m.m20) / S;
	qx = (m.m01 + m.m10) / S; 
	qy = 0.25f * S;
	qz = (m.m12 + m.m21) / S; 
	} else { 
	float S = sqrt(1.0f + m.m22 - m.m00 - m.m11) * 2.0f; // S=4*qz
	qw = (m.m10 - m.m01) / S;
	qx = (m.m02 + m.m20) / S;
	qy = (m.m12 + m.m21) / S;
	qz = 0.25f * S;
	}
	*qq = quat(qw, qx, qy, qz);
}

CIMGUI_API void quat_pos_cast(float f[16], quat *qq, vec3 *pos)
{
	quat_cast(f,qq);
	pos->x = f[12];
	pos->y = f[13];
	pos->z = f[14];
}


