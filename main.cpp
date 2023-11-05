#include "glad.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <cstddef>
#include <sstream>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/backends/imgui_impl_glfw.h"
void framebuffer_callback(GLFWwindow* window,int,int);
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
struct {
    unsigned int cpt;
    unsigned int quadFrag;
    unsigned int quadVert;
} shaders;
struct{
    unsigned int particlePosition;
    unsigned int particleVelocity;
    unsigned int particlePressure;
    unsigned int neighborGrid;
    unsigned int quadIndices;
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
int main(){
    glfwInit();
    GLFWwindow* window=glfwCreateWindow(400, 400, "glfluid", NULL,NULL);
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);
    glfwMakeContextCurrent(window);

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();
    if(!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)){
        spdlog::error("failed to init glad");
        return -1;
    }
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
void render(){
    glUseProgram(RenderTargetProgram);
    static const float black[]={0,0,0,0};
    glClearBufferfv(GL_COLOR,0,black);
    glBindVertexArray(SquareVAO);
    glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
}
void render_imgui(){
    ImGui::Begin("GLFluid");
    ImGui::Text("opengl fluid");
    ImGui::End();
}
void init(){
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
    glBindVertexArray(0);

    //quad shader
    glCreateTextures(GL_TEXTURE_2D,1,&textures.renderTarget);
    glTextureStorage2D(textures.renderTarget,1,GL_RGBA,1024,1024);// 1024 * 1024
    glBindTextureUnit(0,textures.renderTarget);// 0 is the texture unit of quad
    glCreateSamplers(1,&samplers.renderTarget);
    glBindSampler(0,samplers.renderTarget);
    
    //pragrams
    ComputeProgram=glCreateProgram();
    shaders.cpt=load_shader("shaders/cpt.comp", GL_COMPUTE_SHADER,ComputeProgram);

    RenderTargetProgram=glCreateProgram();
    shaders.quadVert=load_shader("shaders/quad.vert", GL_VERTEX_SHADER, RenderTargetProgram);
    shaders.quadFrag=load_shader("shaders/quad.frag", GL_FRAGMENT_SHADER, RenderTargetProgram);

    //particlePosition init
    glGenBuffers(1,&buffers.particlePosition);
    glBindBuffer(GL_ARRAY_BUFFER,buffers.particlePosition);
    particle_properties.position=new vec4[1024];
    glBufferData(buffers.particlePosition,1024*sizeof(float)*4,NULL,NULL);//1024 particles. 4 component per position vector
    glCreateTextures(GL_TEXTURE_BUFFER,1,&textures.particlePosition);
    glTextureBuffer(textures.particlePosition,GL_RGBA32F,buffers.particlePosition);
    glBindImageTexture(0,textures.particlePosition,0,GL_TRUE,0,GL_READ_WRITE,GL_RGBA32F);//0 is the image unit.
    
    //link compute program
    link_program(ComputeProgram);
    spdlog::info("Init Sucess");
    link_program(RenderTargetProgram);
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