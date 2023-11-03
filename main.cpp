#include "glad.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <cstddef>
#include <sstream>
void framebuffer_callback(GLFWwindow* window,int,int);
unsigned int load_shader(const char * filePath, GLenum shaderType);
void bind_uniformi(const char * uniformName,float val);
void render();
void update_particles();
void init();
unsigned int SquareVBO,SquareVAO,ShaderProgram;
static float square[4][4]{
    {0,0},
    {1,0},
    {1,1},
    {0,1}
};
struct {
    unsigned int cpt;
} shaders;
struct{
    unsigned int particlePosition;
    unsigned int particleVelocity;
    unsigned int particlePressure;
    unsigned int neighborGrid;
} buffers;
struct {
    unsigned int particlePosition;
    unsigned int particleVelocity;
    unsigned int particlePressure;
    unsigned int neighborGrid;
} textures;
int main(){
    glfwInit();
    GLFWwindow* window=glfwCreateWindow(400, 400, "glfluid", NULL,NULL);
    glfwSetFramebufferSizeCallback(window, framebuffer_callback);
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)){
        spdlog::error("failed to init glad");
        return -1;
    }
    init();
    while(!glfwWindowShouldClose(window)){
        update_particles();
        render();
        glfwSwapBuffers(window);
        glfwPollEvents();
        
    }
    glfwTerminate();
    return 0;
}
void framebuffer_callback(GLFWwindow* window,int width,int height){ glViewport(0, 0, width, height); }
void render(){
    
}
void init(){
    glCreateBuffers(1,&SquareVBO);
    glNamedBufferStorage(SquareVBO,sizeof(square),square,NULL);
    glCreateVertexArrays(1,&SquareVAO);
    glBindVertexArray(SquareVAO);
    glBindBuffer(GL_VERTEX_ARRAY,SquareVBO);
    glVertexAttribPointer(0,sizeof(float),GL_FLOAT,GL_FALSE,0,0);
    glBindVertexArray(0);

    ShaderProgram=glCreateProgram();
    
    shaders.cpt=load_shader("cpt.comp", GL_COMPUTE_SHADER);

    
    //particlePosition init
    glGenBuffers(1,&buffers.particlePosition);
    glBindBuffer(GL_ARRAY_BUFFER,buffers.particlePosition);
    glNamedBufferStorage(buffers.particlePosition,1024,NULL,NULL);
    glCreateTextures(GL_TEXTURE_BUFFER,1,&textures.particlePosition);
    glTextureBuffer(textures.particlePosition,GL_RGBA32F,buffers.particlePosition);
    glBindImageTexture(0,textures.particlePosition,0,GL_TRUE,0,GL_READ_WRITE,GL_RGBA32F);//0 is the image unit.
    bind_uniformi("particlePosition", 0);

    //the global invocation id is the id of a particle. the shader can read other particle through the id.
    //neighbor grid stores the id. neightborGrid
    
    //link program
    glLinkProgram(ShaderProgram);
    char* log = new char[1000];
    glGetProgramInfoLog(ShaderProgram,1000,NULL,log);
    spdlog::info("Link Shader Log:{}",log);
    glUseProgram(ShaderProgram);

    spdlog::info("Init Sucess");
    delete [] log;
}
unsigned int load_shader(const char * filePath,GLenum shaderType){
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
    glAttachShader(ShaderProgram,ShaderObj);
    delete [] log;
    return ShaderObj;
}
void update_particles(){
    glDispatchCompute(10,1,1);
    
}
void bind_uniformi(const char * uniformName,float val){
    GLuint loc=glGetUniformLocation(ShaderProgram,uniformName);
    if(loc==-1)
        spdlog::error("Uniform {} does not exist", uniformName);
    else
        glUniform1f(loc,val);
}