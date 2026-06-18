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

#include "cimplot3d.h"
#include<math.h>

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

static void error_callback(int e, const char *d)
{printf("Error %d: %s\n", e, d);}

/* Platform */
static GLFWwindow *win;
struct ImGuiContext* ctx;
struct ImGuiIO* io;

struct ImPlot3DContext* ctx_plot;
float xs1[1001], ys1[1001], zs1[1001];

#define size_x 95
#define size_y 95
#define total_points  size_x * size_y
float xs[total_points], ys[total_points],shapevals[total_points];


void gui_init() {
    // IMGUI_CHECKVERSION();
    ctx = igCreateContext(NULL);

    ctx_plot = ImPlot3D_CreateContext();

    io  = igGetIO();

    const char* glsl_version = "#version 330 core";
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Setup style
    igStyleColorsDark(NULL);
	
	//data
	int count = 0;
	for (int i=0;i < size_x; i++) { 
		for (int j=0;j < size_y; j++) {
			xs[count] = i;
			ys[count] = j;
			shapevals[count] = i*j;
			count++;
		}
	}
}

void gui_terminate() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImPlot3D_DestroyContext(ctx_plot);

    igDestroyContext(ctx);
}

void gui_render() {
    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}


	
void gui_update() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    igNewFrame();
	
	for (int i = 0;i<1001;i++) {
        xs1[i] = i * 0.001;
        ys1[i] = 0.5 + 0.5 * cos(50 * (xs1[i] + igGetTime() / 10));
        zs1[i] = 0.5 + 0.5 * sin(50 * (xs1[i] + igGetTime() / 10));
    }
    //ig.ImPlot3D_ShowDemoWindow()
	ImPlot3DSpec *specp = ImPlot3DSpec_ImPlot3DSpec();
	ImPlot3DSpec spec = *specp;
    igBegin("Ploters", NULL, 0);
    if (ImPlot3D_BeginPlot("Plot Line", *ImVec2_ImVec2_Float(-1,0), 0)) {
        ImPlot3D_PlotLine_FloatPtr("f(x)", xs1, ys1, zs1, 1001, spec);
        ImPlot3D_EndPlot();
    }
    if (ImPlot3D_BeginPlot("Plot surface", *ImVec2_ImVec2_Float(-1,0), 0)) {
        ImPlot3D_PlotSurface_FloatPtr("g(x,y)", xs, ys, shapevals, size_x, size_y,0,0,spec);
        ImPlot3D_EndPlot();
    }
    igEnd();
	 ImPlot3DSpec_destroy(specp);

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