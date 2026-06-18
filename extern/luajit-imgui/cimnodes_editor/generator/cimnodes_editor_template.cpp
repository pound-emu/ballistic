#include "imgui.h"
#include "imgui_internal.h"
#include "./imgui-node-editor/imgui_node_editor.h"
#include "cimnodes_editor.h"
#include <cstring>


#include "auto_funcs.cpp"

///manuals
CIMGUI_API NodeId* ax_NodeEditor_NodeId(uintptr_t val)
{
	return IM_NEW(NodeId)(val);
}
CIMGUI_API void ax_NodeEditor_NodeId_destroy(NodeId* self)
{
	return IM_DELETE(self);
}
CIMGUI_API PinId* ax_NodeEditor_PinId(uintptr_t val)
{
	return IM_NEW(PinId)(val);
}
CIMGUI_API void ax_NodeEditor_PinId_destroy(PinId* self)
{
	return IM_DELETE(self);
}
CIMGUI_API LinkId* ax_NodeEditor_LinkId(uintptr_t val)
{
	return IM_NEW(LinkId)(val);
}
CIMGUI_API void ax_NodeEditor_LinkId_destroy(LinkId* self)
{
	return IM_DELETE(self);
}
CIMGUI_API uintptr_t ax_NodeEditor_NodeId_value(NodeId* self)
{
	return self->Get();
}
CIMGUI_API uintptr_t ax_NodeEditor_PinId_value(PinId* self)
{
	return self->Get();
}
CIMGUI_API uintptr_t ax_NodeEditor_LinkId_value(LinkId* self)
{
	return self->Get();
}



