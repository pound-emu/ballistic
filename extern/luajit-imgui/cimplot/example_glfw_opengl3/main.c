#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#ifdef _MSC_VER
#include <windows.h>
#endif
#include <GL/gl.h>

#define igGetIO igGetIO_Nil

#include "cimplot.h"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

static void error_callback(int e, const char *d)
{printf("Error %d: %s\n", e, d);}

/* Platform */
static GLFWwindow *win;
struct ImGuiContext* ctx;
struct ImGuiIO* io;

struct ImPlotContext* ctx_plot;
    
void gui_init() {
    // IMGUI_CHECKVERSION();
    ctx = igCreateContext(NULL);

    ctx_plot = ImPlot_CreateContext();

    io  = igGetIO();

    const char* glsl_version = "#version 330 core";
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Setup style
    igStyleColorsDark(NULL);
}

void gui_terminate() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImPlot_DestroyContext(ctx_plot);

    igDestroyContext(ctx);
}

void gui_render() {
    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}


void ImPlotPoint_getterA(void* data, int idx, ImPlotPoint *pp)
{
	ImS64 *datat = (ImS64*)data;
	pp->x = idx;
	pp->y = datat[idx];
}

ImPlotPoint ImPlotPoint_getterB(int idx, void* data)
{
	ImS64 *datat = (ImS64*)data;
	ImPlotPoint pp;// = *ImPlotPoint_ImPlotPoint_Nil();
	pp.x = idx;
	pp.y = datat[idx];
	return pp;
}


	
void gui_update() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    igNewFrame();

    ImS64   bar_data[11];
    // Populate the array with values from 0 to 10
    for (int i = 0; i < 11; i++) {
        bar_data[i] = i;  // Assign values from 0 to 10
    }

    // Set plot size and flags
    ImVec2 plot_size;
    plot_size.x = 500;
    plot_size.y = 300; // Set plot size (width x height)
    
	ImPlotSpec *specp = ImPlotSpec_ImPlotSpec();
	ImPlotSpec spec = *specp;
	//ImPlotSpec spec;
	// spec.LineColor.x = 1;
	// spec.LineColor.y = 1;
	// spec.LineColor.z = 0;
	// spec.LineColor.w = 1;
	// spec.LineWeight = 1.0f;
	// spec.FillColor.x = 1;
	// spec.FillColor.y = 0.5f;
	// spec.FillColor.z = 0;
	// spec.FillColor.w = 1;
	// spec.FillAlpha = 0.5f;
	// spec.Marker = ImPlotMarker_Square;
	// spec.Size = 6;
	// spec.Stride = sizeof(ImS64);
	// spec.Flags = ImPlotItemFlags_NoLegend | ImPlotLineFlags_Shaded;
	
    igBegin("My Window", NULL, 0);
    if (ImPlot_BeginPlot("My Plot",plot_size,0)) { //THIS BREAKS
        ImPlot_PlotBars_S64PtrInt("My Bar Plot", bar_data, 11,0.67,0,spec);// 0,0,sizeof(ImS64));
		//ImPlot_PlotBarsG("My Bar PlotG", ImPlotPoint_getterA, bar_data,11,0,spec);// 0,0,sizeof(ImS64));
		ImPlot_PlotBarsG("My Bar PlotH", ImPlotPoint_getterB, bar_data,11,0,spec);// 0,0,sizeof(ImS64));
        ImPlot_EndPlot();
    }
    ImPlotSpec_destroy(specp);
    //igText("Test");
    //igButton("Test",(struct ImVec2){200,50});

    igEnd();

    // // Normally user code doesn't need/want to call this because positions are saved in .ini file anyway. 
    // // Here we just want to make the demo initial state a bit more friendly!
    // igSetNextWindowPos((struct ImVec2){0,0}, ImGuiCond_FirstUseEver,(struct ImVec2){0,0} ); 
    //igShowDemoWindow(NULL);
    //ImPlot_ShowDemoWindow(NULL);
}


int main(int argc, char** argv) {

    /* GLFW */
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        fprintf(stdout, "[GFLW] failed to init!\n");
        return -1;//exit(1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    win = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "CIMGUI", NULL, NULL);
    glfwMakeContextCurrent(win);
	//done by IMGUI_IMPL_OPENGL_LOADER_GL3W definition
    // bool err = gl3wInit();
    // if (err){
        // fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        // return 1;
    // }
    
    gui_init();

    // glfwSetWindowSizeCallback(win, onResize);
    int width, height;
    glfwGetWindowSize(win, &width, &height);
     
    /* OpenGL */
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    
    while (!glfwWindowShouldClose(win)) {
        /* Input */
        glfwPollEvents();
    
        gui_update();

        /* Draw */
        glfwGetWindowSize(win, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.0, 0.0, 0.0, 0.0);

        gui_render();

        glfwSwapBuffers(win);
    }

    gui_terminate();
    glfwTerminate();

}