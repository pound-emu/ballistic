
#include "./implot/implot.h"
#include "./implot/implot_internal.h"
#include "cimplot.h"

//ImPlotPoint getters manually wrapped for taking getters modifying ImPlotPoint*

ImPlotPoint_getter getter_funcX;
ImPlotPoint_getter getter_funcX2;

ImPlotPoint Wrapper(int idx, void* data)
{
	ImPlotPoint pp;
	getter_funcX(data, idx, reinterpret_cast<ImPlotPoint_c*>(&pp));
	return pp;
}

ImPlotPoint Wrapper2(int idx, void* data)
{
	ImPlotPoint pp;
	getter_funcX2(data, idx, reinterpret_cast<ImPlotPoint_c*>(&pp));
	return pp;
}

#include "auto_funcs.cpp"
