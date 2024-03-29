#include "glad.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
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
template<typename T>
T* mapDataFromServer(int dataNumber, unsigned int __array){
    glBindBuffer(GL_ARRAY_BUFFER,__array);
    return (T *) 
        glMapBufferRange(GL_ARRAY_BUFFER,0,dataNumber * sizeof(T),GL_MAP_WRITE_BIT);
}
void unmapData(){
    glUnmapBuffer(GL_ARRAY_BUFFER);
}
void link_program(unsigned int program);
void bind_uniformi(const char * uniformName,float val,unsigned int program);
void render();
void render_imgui();
void update_particles(float deltaTime);
void bitonic_sort();
void init();
unsigned int SquareVBO,ComputeProgram,RenderTargetProgram;
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
    unsigned int particleInit;
    unsigned int particleUpdate;
    unsigned int quadFrag;
    unsigned int quadVert;
    unsigned int sort;
} shaders;
struct {
    unsigned int computeVAO;
    unsigned int quad;
    unsigned int sort;
} VAOs;
struct{
    unsigned int particlePosition;
    unsigned int particleVelocity;
    unsigned int particleDensity;
    unsigned int neighborGrid;
    unsigned int quadIndices;
    unsigned int quadCoord;
    unsigned int partileIdxAndCell;
    unsigned int numberOfParticlePerCell;
} buffers;
struct {
    unsigned int particlePosition;
    unsigned int particleVelocity;
    unsigned int particleDensity;
    unsigned int neighborGrid;
    unsigned int renderTarget;
} textures;
struct {
    unsigned int renderTarget;
} samplers;
struct {
    vec4* position;
    vec4* velocity;
} particle_properties;
struct{
    //32 * 32 = 1024
    int width=32;
    int length=32;
    int interval=20;
    float gravity=-9.8;
    float mass=5;
    float size=5;
    float accler_scale=10;// multiply a scale constant with the accler 
    float kernel_radius=10;
} particle_system_properties;
struct{
    unsigned int particleInit;
    unsigned int sort;
} programs;
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
    float currentTime=glfwGetTime();
    float lastTime=currentTime-.01;
    
    glUseProgram(programs.particleInit);
    glDispatchCompute(64,1,1);

    while(!glfwWindowShouldClose(window)){
        float deltaTime=currentTime-lastTime;
        lastTime=currentTime;
        update_particles(deltaTime);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        render_imgui();
        ImGui::Render();
        render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
        currentTime=glfwGetTime();
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
    glBindVertexArray(VAOs.quad);
    glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
}
void render_imgui(){
    ImGui::Begin("GLFluid");
    ImGui::SliderInt("width", &particle_system_properties.width, 0, 32);
    ImGui::SliderInt("length", &particle_system_properties.length, 0, 32);
    ImGui::SliderFloat("particle size", &particle_system_properties.size, 0, 10);
    ImGui::Text("Render Target: %u",textures.renderTarget);
    ImGui::SliderFloat("Gravity", &particle_system_properties.gravity, -10, 10);
    ImGui::SliderFloat("Mass", &particle_system_properties.mass, .01, 30);
    ImGui::SliderFloat("Acceleration Scale", &particle_system_properties.accler_scale, 3, 10);
    ImGui::SliderFloat("Kernel Radius",&particle_system_properties.kernel_radius,10,100);
    ImGui::End();
}
void init(){
    //----renderTarget----
    //quad
    static unsigned int indices[6]{
        0,1,3,1,2,3
    };
    glCreateVertexArrays(1,&VAOs.quad);
    glBindVertexArray(VAOs.quad);

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

    glBindImageTexture(1,textures.renderTarget,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F);//bind the image unit for render target

    //---particlePosition init
    glCreateBuffers(1,&buffers.particlePosition);
    glBindBuffer(GL_ARRAY_BUFFER,buffers.particlePosition);
    glBufferData(GL_ARRAY_BUFFER,1024*sizeof(vec4),NULL,GL_DYNAMIC_COPY);//1024 particles. 4 component per position vector
    particle_properties.position=(vec4 *)
        glMapBufferRange(GL_ARRAY_BUFFER,0,1024 * sizeof(vec4),GL_MAP_WRITE_BIT);
/*     vec4 starting_pos;
    starting_pos.x=256;
    starting_pos.y=768;
    for (int i = 0; i < particle_system_properties.width; i++) {
      for (int j = 0; j < particle_system_properties.length; j++) {
        vec4 pos;
        pos.x = starting_pos.x + j * particle_system_properties.interval;
        pos.y = starting_pos.y - i * particle_system_properties.interval;
        pos.z = 0;
        pos.w = 0;
        particle_properties
            .position[i * particle_system_properties.length + j] = pos;
      }
    }*/
    for(int i = 0; i < 1024; i++){
        vec4 pos;
        pos.x = rand()%1024;
        pos.y = rand()%1024;
        pos.z = 0;
        pos.w = 0;
        particle_properties.position[i] = pos;
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);

    glCreateTextures(GL_TEXTURE_BUFFER,1,&textures.particlePosition);
    //glBindTexture(GL_TEXTURE_BUFFER,textures.particlePosition);
    glTextureBuffer(textures.particlePosition,GL_RGBA32F,buffers.particlePosition);
    glBindImageTexture(0,textures.particlePosition,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F);//0 is the image unit.

    //density
    glCreateBuffers(1, &buffers.particleDensity);
    glBindBuffer(GL_ARRAY_BUFFER, buffers.particleDensity);
    glBufferData(GL_ARRAY_BUFFER,1024*sizeof(float),NULL,GL_DYNAMIC_COPY);
    glCreateTextures(GL_TEXTURE_BUFFER,1,&textures.particleDensity);
    glTextureBuffer(textures.particleDensity,GL_R32F,buffers.particleDensity);
    glBindImageTexture(4,textures.particleDensity,0,GL_FALSE,0,GL_READ_WRITE,GL_R32F);//0 is the image unit.

    glBindVertexArray(0);
    //TODO: encapsulate these behaviours into a function
    
    // map the partcile index to the linear index of the cell
    // (particleIndex, linearIndexOfCell)
    glCreateVertexArrays(1,&VAOs.sort);
    glBindVertexArray(VAOs.sort);
    glCreateBuffers(1,&buffers.partileIdxAndCell);
    glBindBuffer(GL_ARRAY_BUFFER,buffers.partileIdxAndCell);
    glBufferData(GL_ARRAY_BUFFER,1024*sizeof(float)*2,NULL,GL_DYNAMIC_COPY);

    unsigned int ptclIdxAndCells_tex;
    glCreateTextures(GL_TEXTURE_BUFFER,1,&ptclIdxAndCells_tex);
    glTextureBuffer(ptclIdxAndCells_tex,GL_RG32F,buffers.partileIdxAndCell);
    glBindImageTexture(3,ptclIdxAndCells_tex,0,GL_FALSE,0,GL_READ_WRITE,GL_RG32F);

    // the number of particles stored in the cell
    // 1 2  3 4 5 CellLinearIndex
    // 4 2 14 2 8 number of the particles stored in the cell
    glCreateBuffers(1,&buffers.numberOfParticlePerCell);
    glBindBuffer(GL_ARRAY_BUFFER,buffers.numberOfParticlePerCell);
    glBufferData(GL_ARRAY_BUFFER,(4*4)*sizeof(float),NULL,GL_DYNAMIC_COPY);
    
    int* nopp_array=(int*) glMapBufferRange(GL_ARRAY_BUFFER,0,(4*4)*sizeof(int),GL_MAP_WRITE_BIT);
    std::for_each(nopp_array,nopp_array+(4*4),[](int& item){item = 0;});
    glUnmapBuffer(GL_ARRAY_BUFFER);

    unsigned int tex_nopp;
    glCreateTextures(GL_TEXTURE_BUFFER,1,&tex_nopp);
    glTextureBuffer(tex_nopp,GL_R32I,buffers.numberOfParticlePerCell);
    glBindImageTexture(5,tex_nopp,0,GL_FALSE,0,GL_READ_WRITE,GL_R32I);
    
    
    //pragrams
    programs.sort = glCreateProgram();
    shaders.sort = 
        load_shader("shaders/sort.comp", GL_COMPUTE_SHADER, programs.sort);
    ComputeProgram = glCreateProgram();
    shaders.particleUpdate =
        load_shader("shaders/particleUpdate.comp", GL_COMPUTE_SHADER, ComputeProgram);
    programs.particleInit = glCreateProgram();
    shaders.particleInit = 
        load_shader("shaders/particleInit.comp", GL_COMPUTE_SHADER, programs.particleInit);
    RenderTargetProgram = glCreateProgram();
    shaders.quadVert =
        load_shader("shaders/quad.vert", GL_VERTEX_SHADER, RenderTargetProgram);
    shaders.quadFrag = load_shader("shaders/quad.frag", GL_FRAGMENT_SHADER,
                                   RenderTargetProgram);

    //link compute program
    link_program(ComputeProgram);
    link_program(RenderTargetProgram);
    link_program(programs.sort);
    link_program(programs.particleInit);
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
void bitonic_sort(){
    glBindVertexArray(VAOs.sort);
    glUseProgram(programs.sort);
    glDispatchCompute(1,1,1);
}
void update_particles(float deltaTime){
    //dispatch the computation to GPU
    glBindVertexArray(VAOs.computeVAO);
    glUseProgram(ComputeProgram);
    float black[4]={0,0,0,0};
    glClearTexImage(textures.renderTarget,0,GL_RGB,GL_FLOAT,black);
    glUniform1f(glGetUniformLocation(ComputeProgram,"particleSize"),particle_system_properties.size);
    glUniform1f(glGetUniformLocation(ComputeProgram,"gravity"),particle_system_properties.gravity);
    glUniform1f(glGetUniformLocation(ComputeProgram,"mass"),particle_system_properties.mass);
    glUniform1f(glGetUniformLocation(ComputeProgram,"deltaTime"),.01);//for debug purpose
    glUniform1f(glGetUniformLocation(ComputeProgram,"acclerScale"),particle_system_properties.accler_scale);
    glUniform1f(glGetUniformLocation(ComputeProgram,"kernelRadius"),particle_system_properties.kernel_radius);
    
    glDispatchCompute(64,1,1);//64 * 16 = 1024. 64 = work group 16 = local work grou size

    bitonic_sort();
    
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
    delete [] log;
}