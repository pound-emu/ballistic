#ifndef BALLISTIC_DASHBOARD_BACKEND_H
#define BALLISTIC_DASHBOARD_BACKEND_H

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    typedef struct GLFWwindow GLFWwindow;
    void                      dashboard_backend_init(GLFWwindow *window);
    void                     *dashboard_backend_get_context(void);
    void                      dashboard_backend_new_frame(void);
    void                      dashboard_backend_render(void);
    void                      dashboard_backend_shutdown(void);
    void                      dashboard_backend_recover(GLFWwindow *window);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // BALLISTIC_DASHBOARD_BACKEND_H

/*** end of file ***/
