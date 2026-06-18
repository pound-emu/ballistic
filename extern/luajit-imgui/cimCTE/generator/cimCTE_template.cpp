#include "imgui.h"
#include "imgui_internal.h"
#include "./ImGuiColorTextEdit/TextEditor.h"
#include "./ImGuiColorTextEdit/TextDiff.h"
#include "cimCTE.h"
#include <cstring>


#include "auto_funcs.cpp"
////////////////manually generated

CIMGUI_API char* TextEditor_GetText_alloc(TextEditor* self)
{
    std::string str = self->GetText();
    char* cStr = (char*)IM_ALLOC(str.size() + 1); // Allocate memory
    std::strcpy(cStr, str.c_str()); // Copy string contents
    return cStr; // Return new C-style string
}
CIMGUI_API void TextEditor_GetText_free(char* ptr)
{
    IM_FREE(ptr); // free memory
}
CIMGUI_API const char* TextEditor_GetText_static(TextEditor* self)
{
    static std::string str = self->GetText();
    return str.c_str();
}
// CIMGUI_API void TextEditor_IterateIdentifiers(TextEditor* self, void(*cb)(const char *))
// {
	// self->IterateIdentifiers([cb](const std::string& identifier) {
		// cb(identifier.c_str());
	// });
// }




