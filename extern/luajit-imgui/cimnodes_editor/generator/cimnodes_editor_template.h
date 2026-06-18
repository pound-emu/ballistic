#ifndef CIMMODES_EDITOR_INCLUDED
#define CIMMODES_EDITOR_INCLUDED

#include "cimgui.h"

#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "imgui_structs.h"
#else
#endif // CIMGUI_DEFINE_ENUMS_AND_STRUCTS

PLACE_STRUCTS_C

#include "auto_funcs.h"

///manuals
CIMGUI_API NodeId* ax_NodeEditor_NodeId(uintptr_t val);
CIMGUI_API void ax_NodeEditor_NodeId_destroy(NodeId* self);
CIMGUI_API PinId* ax_NodeEditor_PinId(uintptr_t val);
CIMGUI_API void ax_NodeEditor_PinId_destroy(PinId* self);
CIMGUI_API LinkId* ax_NodeEditor_LinkId(uintptr_t val);
CIMGUI_API void ax_NodeEditor_LinkId_destroy(LinkId* self);
CIMGUI_API uintptr_t ax_NodeEditor_NodeId_value(NodeId* self);
CIMGUI_API uintptr_t ax_NodeEditor_PinId_value(PinId* self);
CIMGUI_API uintptr_t ax_NodeEditor_LinkId_value(LinkId* self);

#endif //CIMMODES_EDITOR_INCLUDED




