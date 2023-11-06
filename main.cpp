#include "glad.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#include <fstream>
#include <cstddef>
#include <sstream>

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/backends/imgui_impl_glfw.h"
void framebuffer_callback(GLFWwindow* window,int,int);
void glfw_error_callback(int, const char *);
void gl_error_callback(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
unsigned int load_shader(const char * filePath, GLenum shaderType,unsigned int program);
void link_program(unsigned int program);
void bind_uniformi(const char * uniformName,float val,unsigned int program);
void render();
void render_imgui();
void update_particles();
void init();
unsigned int SquareVBO,SquareVAO,ComputeProgram,RenderTargetProgram;
struct vec4{
    float x;
    float y;
    float z;
    float w;
};
static float square[4][2]{
    {-1,1},
    {1,1},
    {1,-1},
    {-1,-1},
};
static float square_uv[4][2]{
    {0,1},
    {1,1},
    {1,0},
    {0,0}
};
struct {
    unsigned int cpt;
    unsigned int quadFrag;
    unsigned int quadVert;
} shaders;
struct {
    unsigned int computeVAO;
} VAOs;
struct{
    unsigned int particlePosition;
    unsigned int particleVelocity;
    unsigned int particlePressure;
    unsigned int neighborGrid;
    unsigned int quadIndices;
    unsigned int quadCoord;
} buffers;
struct {
    unsigned int particlePosition;
    unsigned int particleVelocity;
    unsigned int particlePressure;
    unsigned int neighborGrid;
    unsigned int renderTarget;
} textures;
struct {
    unsigned int renderTarget;
} samplers;
struct {
    vec4* position;
} particle_properties;
struct{
    //32 * 32 = 1024
    int width=32;
    int length=32;
    int interval=10;
    float size=5;
} particle_system_properties;
int main(){
    glfwInit();

    GLFWwindow* window=glfwCreateWindow(512, 512, "glfluid", NULL,NULL);
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);
    glfwSetErrorCallback(glfw_error_callback);
    
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)){
        spdlog::error("failed to init glad");
        return -1;
    }

    glDebugMessageCallback(gl_error_callback,NULL);

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();
    
    init();
    while(!glfwWindowShouldClose(window)){
        update_particles();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        render_imgui();
        ImGui::Render();
        render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
        
    }
    
    glfwTerminate();
    return 0;
}
void framebuffer_callback(GLFWwindow* window,int width,int height){ glViewport(0, 0, width, height); }
void glfw_error_callback(int error_code, const char * message){ spdlog::error("glfw error{}: {}",error_code,message);}
void gl_error_callback(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam){
    spdlog::error("GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
    ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message);
}
void render(){
    glUseProgram(RenderTargetProgram);
    static const float black[]={0,0,0,0};
    glClearBufferfv(GL_COLOR,0,black);
    glBindVertexArray(SquareVAO);
    glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
}
void render_imgui(){
    ImGui::Begin("GLFluid");
    ImGui::SliderInt("width", &particle_system_properties.width, 0, 32);
    ImGui::SliderInt("length", &particle_system_properties.length, 0, 32);
    //set the particle size
    ImGui::SliderFloat("particle size", &particle_system_properties.size, 0, 10);
    ImGui::Text("Render Target: %u",textures.renderTarget);
    glUniform1f(glGetUniformLocation(ComputeProgram,"particleSize"),particle_system_properties.size);
    ImGui::End();
}
void init(){
    //----renderTarget----
    //quad
    static unsigned int indices[6]{
        0,1,3,1,2,3
    };
    glCreateVertexArrays(1,&SquareVAO);
    glBindVertexArray(SquareVAO);

    glCreateBuffers(1,&SquareVBO);
    glNamedBufferStorage(SquareVBO,sizeof(float)*8,square,NULL);
    glBindBuffer(GL_ARRAY_BUFFER,SquareVBO);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);// 0 is the layout position of quad.frag
    glEnableVertexAttribArray(0);

    glCreateBuffers(1,&buffers.quadIndices);
    glNamedBufferStorage(buffers.quadIndices,sizeof(unsigned int)*6,indices,NULL);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers.quadIndices);

    //quad uv
    glCreateBuffers(1,&buffers.quadCoord);
    glNamedBufferStorage(buffers.quadCoord,sizeof(float)*8,square_uv,NULL);
    glBindBuffer(GL_ARRAY_BUFFER,buffers.quadCoord);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,0,0);
    glEnableVertexAttribArray(1);
    
    //quad shader
    glCreateTextures(GL_TEXTURE_2D,1,&textures.renderTarget);
    glTextureStorage2D(textures.renderTarget,1,GL_RGBA32F,1024,1024);// 1024 * 1024
    glBindTextureUnit(0,textures.renderTarget);// 0 is the texture unit of quad
    glCreateSamplers(1,&samplers.renderTarget);
    glBindSampler(0,samplers.renderTarget);
    
    glBindVertexArray(0);
    
    //-----compute shader-----
    glCreateVertexArrays(1,&VAOs.computeVAO);
    glBindVertexArray(VAOs.computeVAO);//bind VAOs for the compute shader

    glBindImageTexture(1,textures.renderTarget,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F);

    //pragrams
    ComputeProgram=glCreateProgram();
    shaders.cpt=load_shader("shaders/cpt.comp", GL_COMPUTE_SHADER,ComputeProgram);

    RenderTargetProgram=glCreateProgram();
    shaders.quadVert=load_shader("shaders/quad.vert", GL_VERTEX_SHADER, RenderTargetProgram);
    shaders.quadFrag=load_shader("shaders/quad.frag", GL_FRAGMENT_SHADER, RenderTargetProgram);

    //particlePosition init
    glCreateBuffers(1,&buffers.particlePosition);
    glBindBuffer(GL_ARRAY_BUFFER,buffers.particlePosition);
    particle_properties.position=new vec4[1024];
    vec4 starting_pos;
    starting_pos.x=-256;
    starting_pos.y=256;
    for(int i = 0;i<particle_system_properties.width;i++){
        for(int j = 0;j<particle_system_properties.length;j++){
            vec4 pos;
            pos.x=starting_pos.x+i*particle_system_properties.interval;
            pos.y=starting_pos.y+j*particle_system_properties.interval;
            particle_properties.position[i*particle_system_properties.length+j]=pos;
        }
    }
    glBufferData(buffers.particlePosition,1024*sizeof(vec4),particle_properties.position,NULL);//1024 particles. 4 component per position vector
    glCreateTextures(GL_TEXTURE_BUFFER,1,&textures.particlePosition);
    glTextureBuffer(textures.particlePosition,GL_RGBA32F,buffers.particlePosition);
    glBindImageTexture(0,textures.particlePosition,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F);//0 is the image unit.
    
    //link compute program
    link_program(ComputeProgram);
    link_program(RenderTargetProgram);
    spdlog::info("Init Sucess");
}
unsigned int load_shader(const char * filePath,GLenum shaderType,unsigned int program){
    unsigned int ShaderObj=glCreateShader(shaderType);
    std::ifstream file;
    file.open(filePath);
    std::stringstream ss;
    ss<<file.rdbuf();
    std::string str=ss.str();
    const char * shaderText=str.c_str();
    file.close();
    glShaderSource(ShaderObj,1,&shaderText,NULL);
    glCompileShader(ShaderObj);
    char * log=new char[1000];
    glGetShaderInfoLog(ShaderObj,1000,NULL,log);
    spdlog::info("Compile Shader Log:{}",log);
    glAttachShader(program,ShaderObj);
    delete [] log;
    return ShaderObj;
}
void update_particles(){
    //dispatch the computation to GPU
    glUseProgram(ComputeProgram);
    glDispatchCompute(64,1,1);//64 * 16 = 1024. 64 = work group 16 = local work grou size
}
void bind_uniformi(const char * uniformName,float val,unsigned int program){
    GLuint loc=glGetUniformLocation(program,uniformName);
    if(loc==-1)
        spdlog::error("Uniform {} does not exist", uniformName);
    else
        glUniform1f(loc,val);
}
void link_program(unsigned int program){

    glLinkProgram(program);
    char* log = new char[1000];
    glGetProgramInfoLog(program,1000,NULL,log);
    spdlog::info("Link Shader Program{} Log:{}",program,log);
}