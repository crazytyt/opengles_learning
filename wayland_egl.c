
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <gc_vdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

struct wl_display *display = NULL;
struct wl_compositor *compositor = NULL;
struct wl_surface *surface;
struct wl_egl_window *egl_window;
struct wl_region *region;
struct wl_shell *shell;
struct wl_shell_surface *shell_surface;

#define TUTORIAL_NAME "OpenGL ES 2.0 Tutorial 1"

EGLDisplay egl_display;
EGLConfig egl_conf;
EGLSurface egl_surface;
EGLContext egl_context;

// Global Variables, attribute and uniform
GLint locVertices     = 0;
GLint locColors       = 0;
GLint locTransformMat = 0;

vdkEGL egl;
int width  = 0;
int height = 0;
int posX   = -1;
int posY   = -1;
int samples = 0;
int frames = 0;

// Global Variables, shader handle and program handle
GLuint vertShaderNum  = 0;
GLuint pixelShaderNum = 0;
GLuint programHandle  = 0;

// Triangle Vertex positions.
const GLfloat vertices[3][2] = {
    { -0.5f, -0.5f},
    {  0.0f,  0.5f},
    {  0.5f, -0.5f}
};

// Triangle Vertex colors.
const GLfloat color[3][3] = {
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f}
};

// Start with an identity matrix.
GLfloat transformMatrix[16] =
{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

void RenderInit()
{
    // Grab location of shader attributes.
    locVertices = glGetAttribLocation(programHandle, "my_Vertex");
    locColors   = glGetAttribLocation(programHandle, "my_Color");
    // Transform Matrix is uniform for all vertices here.
    locTransformMat = glGetUniformLocation(programHandle, "my_TransformMatrix");

    // enable vertex arrays to push the data.
    glEnableVertexAttribArray(locVertices);
    glEnableVertexAttribArray(locColors);

    // set data in the arrays.
    glVertexAttribPointer(locVertices, 2, GL_FLOAT, GL_FALSE, 0, &vertices[0][0]);
    glVertexAttribPointer(locColors, 3, GL_FLOAT, GL_FALSE, 0, &color[0][0]);
    glUniformMatrix4fv(locTransformMat, 1, GL_FALSE, transformMatrix);
}

// Actual rendering here.
void Render()
{
    static float angle = 0;

    // Clear background.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up rotation matrix rotating by angle around y axis.
    transformMatrix[0] = transformMatrix[10] = (GLfloat)cos(angle);
    transformMatrix[2] = (GLfloat)sin(angle);
    transformMatrix[8] = -transformMatrix[2];
    angle += 0.1f;

    /*According to Linux programer's manual, need to clamp the input value of sin or cos to valid value,
        otherwise return NaN and exception may be raised.*/
    if(angle >= 6.3)
    {
        angle = fmod(angle, (float)6.3);
    }

    glUniformMatrix4fv(locTransformMat, 1, GL_FALSE, transformMatrix);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    // flush all commands.
    glFlush ();
    // swap display with drawn surface.
    vdkSwapEGL(&egl);
}

void RenderCleanup()
{
    // cleanup
    glDisableVertexAttribArray(locVertices);
    glDisableVertexAttribArray(locColors);
}

/***************************************************************************************
***************************************************************************************/

// Compile a vertex or pixel shader.
// returns 0: fail
//         1: success
int CompileShader(const char * FName, GLuint ShaderNum)
{
    FILE * fptr = NULL;
#ifdef UNDER_CE
    static wchar_t buffer[MAX_PATH + 1];
    int i = GetModuleFileName(NULL, buffer, MAX_PATH);
    while (buffer[i - 1] != L'\\') i--;
    while (*FName != '\0') buffer[i++] = (wchar_t)(*FName++);
    buffer[i] = L'\0';
    fptr = _wfopen(buffer, L"rb");
#else
    fptr = fopen(FName, "rb");
#endif
    if (fptr == NULL)
    {
        fprintf(stderr, "Cannot open file '%s'\n", FName);
        return 0;
    }

    int length;
    fseek(fptr, 0, SEEK_END);
    length = ftell(fptr);
    fseek(fptr, 0 ,SEEK_SET);

    char * shaderSource = (char*)malloc(sizeof (char) * length);
    if (shaderSource == NULL)
    {
        fprintf(stderr, "Out of memory.\n");
        return 0;
    }

    fread(shaderSource, length, 1, fptr);

    glShaderSource(ShaderNum, 1, (const char**)&shaderSource, &length);
    glCompileShader(ShaderNum);

    free(shaderSource);
    fclose(fptr);

    GLint compiled = 0;
    glGetShaderiv(ShaderNum, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        // Retrieve error buffer size.
        GLint errorBufSize, errorLength;
        glGetShaderiv(ShaderNum, GL_INFO_LOG_LENGTH, &errorBufSize);

        char * infoLog = (char*)malloc(errorBufSize * sizeof(char) + 1);
        if (!infoLog)
        {
            // Retrieve error.
            glGetShaderInfoLog(ShaderNum, errorBufSize, &errorLength, infoLog);
            infoLog[errorBufSize + 1] = '\0';
            fprintf(stderr, "%s\n", infoLog);

            free(infoLog);
        }
        fprintf(stderr, "Error compiling shader '%s'\n", FName);
        return 0;
    }

    return 1;
}

/***************************************************************************************
***************************************************************************************/

// Wrapper to load vetex and pixel shader.
void LoadShaders(const char * vShaderFName, const char * pShaderFName)
{
    vertShaderNum = glCreateShader(GL_VERTEX_SHADER);
    pixelShaderNum = glCreateShader(GL_FRAGMENT_SHADER);

    if (CompileShader(vShaderFName, vertShaderNum) == 0)
    {
        return;
    }

    if (CompileShader(pShaderFName, pixelShaderNum) == 0)
    {
        return;
    }

    programHandle = glCreateProgram();

    glAttachShader(programHandle, vertShaderNum);
    glAttachShader(programHandle, pixelShaderNum);

    glLinkProgram(programHandle);
    // Check if linking succeeded.
    GLint linked = false;
    glGetProgramiv(programHandle, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        // Retrieve error buffer size.
        GLint errorBufSize, errorLength;
        glGetShaderiv(programHandle, GL_INFO_LOG_LENGTH, &errorBufSize);

        char * infoLog = (char*)malloc(errorBufSize * sizeof (char) + 1);
        if (!infoLog)
        {
            // Retrieve error.
            glGetProgramInfoLog(programHandle, errorBufSize, &errorLength, infoLog);
            infoLog[errorBufSize + 1] = '\0';
            fprintf(stderr, "%s", infoLog);

            free(infoLog);
        }

        fprintf(stderr, "Error linking program\n");
        return;
    }
    glUseProgram(programHandle);
}

// Cleanup the shaders.
void DestroyShaders()
{
    glDeleteShader(vertShaderNum);
    glDeleteShader(pixelShaderNum);
    glDeleteProgram(programHandle);
    glUseProgram(0);
}
static void
global_registry_handler(void *data, struct wl_registry *registry, uint32_t id,
	       const char *interface, uint32_t version)
{
    printf("Got a registry event for %s id %d\n", interface, id);
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, 
				      id, 
				      &wl_compositor_interface, 
				      1);
    } else if (strcmp(interface, "wl_shell") == 0) {
	shell = wl_registry_bind(registry, id,
				 &wl_shell_interface, 1);
	
    }
}

static void
global_registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
    printf("Got a registry losing event for %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover
};

static void create_opaque_region() {
    region = wl_compositor_create_region(compositor);
    wl_region_add(region, 0, 0,
		  480,
		  360);
    wl_surface_set_opaque_region(surface, region);
}

static void create_window() {

    egl_window = wl_egl_window_create(surface,
			      480, 360);
    if (egl_window == EGL_NO_SURFACE) {
	fprintf(stderr, "Can't create egl window\n");
	exit(1);
    } else {
	fprintf(stderr, "Created egl window\n");
    }

    egl_surface =
	eglCreateWindowSurface(egl_display,
			       egl_conf,
			       egl_window, NULL);

    if (eglMakeCurrent(egl_display, egl_surface,
		       egl_surface, egl_context)) {
	fprintf(stderr, "Made current\n");
    } else {
	fprintf(stderr, "Made current failed\n");
    }

    Render();
    /*
    glClearColor(1.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
    */

    if (eglSwapBuffers(egl_display, egl_surface)) {
	fprintf(stderr, "Swapped buffers\n");
    } else {
	fprintf(stderr, "Swapped buffers failed\n");
    }
}

static void init_egl() {
    EGLint major, minor, count, n, size;
    EGLConfig *configs;
    int i;
    EGLint config_attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE
    };

    static const EGLint context_attribs[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
    };

    
    egl_display = eglGetDisplay((EGLNativeDisplayType) display);
    if (egl_display == EGL_NO_DISPLAY) {
	fprintf(stderr, "Can't create egl display\n");
	exit(1);
    } else {
	fprintf(stderr, "Created egl display\n");
    }

    if (eglInitialize(egl_display, &major, &minor) != EGL_TRUE) {
	fprintf(stderr, "Can't initialise egl display\n");
	exit(1);
    }
    printf("EGL major: %d, minor %d\n", major, minor);

    eglGetConfigs(egl_display, NULL, 0, &count);
    printf("EGL has %d configs\n", count);

    configs = calloc(count, sizeof *configs);
    
    eglChooseConfig(egl_display, config_attribs,
			  configs, count, &n);
    
    for (i = 0; i < n; i++) {
	eglGetConfigAttrib(egl_display,
			   configs[i], EGL_BUFFER_SIZE, &size);
	printf("Buffer size for config %d is %d\n", i, size);
	eglGetConfigAttrib(egl_display,
			   configs[i], EGL_RED_SIZE, &size);
	printf("Red size for config %d is %d\n", i, size);
	
	// just choose the first one
	egl_conf = configs[i];
	break;
    }

    egl_context =
	eglCreateContext(egl_display,
			 egl_conf,
			 EGL_NO_CONTEXT, context_attribs);

}

static void get_server_references(void) {

    display = wl_display_connect(NULL);
    if (display == NULL) {
	fprintf(stderr, "Can't connect to display\n");
	exit(1);
    }
    printf("connected to display\n");

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (compositor == NULL || shell == NULL) {
	fprintf(stderr, "Can't find compositor or shell\n");

	exit(1);
    } else {
	fprintf(stderr, "Found compositor and shell\n");
    }
}

int main(int argc, char **argv) {

    bool pauseFlag = false;
    //===================================================================
    // EGL configuration - we use 24-bpp render target and a 16-bit Z buffer.
    EGLint configAttribs[] =
    {
        EGL_SAMPLES,      0,
        EGL_RED_SIZE,     8,
        EGL_GREEN_SIZE,   8,
        EGL_BLUE_SIZE,    8,
        EGL_ALPHA_SIZE,   EGL_DONT_CARE,
        EGL_DEPTH_SIZE,   0,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE,
    };

    EGLint attribListContext[] =
    {
        // Needs to be set for es2.0 as default client version is es1.1.
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };


    // Set multi-sampling.
    configAttribs[1] = samples;

    get_server_references();

    surface = wl_compositor_create_surface(compositor);
    if (surface == NULL) {
	fprintf(stderr, "Can't create surface\n");
	exit(1);
    } else {
	fprintf(stderr, "Created surface\n");
    }

    shell_surface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_set_toplevel(shell_surface);

    create_opaque_region();
    //init_egl();


    // Initialize VDK, EGL, and GLES.
    if (!vdkSetupEGL(posX, posY, width, height, configAttribs, NULL, attribListContext, &egl))
    {
        return 1;
    }

    create_window();
    // Adjust the window size to make sure these size values does not go beyound the screen limits.
    vdkGetWindowInfo(egl.window, NULL, NULL, &width, &height, NULL, NULL);

    // Set window title and show the window.
    vdkSetWindowTitle(egl.window, TUTORIAL_NAME);
    vdkShowWindow(egl.window);

    // load and compiler vertex/fragment shaders.
    LoadShaders("vs_es20t1.vert", "ps_es20t1.frag");

    if (programHandle != 0)
    {
        RenderInit();

        int frameCount = 0;
        unsigned int start = vdkGetTicks();


	    while (wl_display_dispatch(display) != -1) {
		;
	    }


        glFinish();
        unsigned int end = vdkGetTicks();
        float fps = frameCount / ((end - start) / 1000.0f);
        printf("%d frames in %d ticks -> %.3f fps\n", frameCount, end - start, fps);

        RenderCleanup();
    }

    // cleanup
    DestroyShaders();
    vdkFinishEGL(&egl);

    wl_display_disconnect(display);
    printf("disconnected from display\n");
    
    exit(0);
}
