#include "common.h"
#include <GLFW/glfw3.h>

int _glfwHasBeenInitialized = 0;

SsResult ssInit(void) {
    if(glfwInit() == GLFW_FALSE)
        return SS_ERROR_MISSED_INITIALIZATION;
    _glfwHasBeenInitialized = 1;
    return SS_SUCCESS;
}
void ssTerminate(void) {
    _glfwHasBeenInitialized = 0;
    glfwTerminate();
}