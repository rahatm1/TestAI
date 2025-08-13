#include <GLFW/glfw3.h>
#include <GL/glu.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <cmath>
#include <vector>
#include <map>
#include <random>
#include <iostream>

struct Vec2 {
    float x, y;
};
struct Vec3 {
    float x, y, z;
};

struct LevelInfo {
    Vec2 holePos;
    std::vector<Vec2> crackVerts;
};

static std::map<int, LevelInfo> levelData;
static std::mt19937 rng(42);

LevelInfo &getLevel(int index) {
    auto it = levelData.find(index);
    if (it != levelData.end()) return it->second;
    std::uniform_real_distribution<float> distPos(-4.0f,4.0f);
    Vec2 pos{distPos(rng), distPos(rng)};
    // generate crack verts
    std::uniform_real_distribution<float> noise(0.7f,1.3f);
    const int segments = 20;
    std::vector<Vec2> verts;
    for(int i=0;i<=segments;i++){
        float ang = (float)i/segments*2*M_PI;
        float r = 0.5f*noise(rng);
        verts.push_back({pos.x + cosf(ang)*r, pos.y + sinf(ang)*r});
    }
    LevelInfo info{pos, verts};
    levelData[index]=info;
    return levelData[index];
}

void drawPlane(float y)
{
    float size=10.0f;
    glColor4f(0.8f,0.9f,1.0f,0.5f);
    glBegin(GL_QUADS);
    glVertex3f(-size,y,-size);
    glVertex3f(size,y,-size);
    glVertex3f(size,y,size);
    glVertex3f(-size,y,size);
    glEnd();
}

void drawWalls(float y)
{
    float size=10.0f; float h=1.0f; float t=0.1f;
    glColor4f(0.8f,0.9f,1.0f,0.5f);
    glBegin(GL_QUADS);
    // +z wall
    glVertex3f(-size,y,-size);
    glVertex3f(size,y,-size);
    glVertex3f(size,y+h,-size);
    glVertex3f(-size,y+h,-size);
    // -z wall
    glVertex3f(-size,y,size);
    glVertex3f(size,y,size);
    glVertex3f(size,y+h,size);
    glVertex3f(-size,y+h,size);
    // +x wall
    glVertex3f(size,y,-size);
    glVertex3f(size,y,size);
    glVertex3f(size,y+h,size);
    glVertex3f(size,y+h,-size);
    // -x wall
    glVertex3f(-size,y,-size);
    glVertex3f(-size,y,size);
    glVertex3f(-size,y+h,size);
    glVertex3f(-size,y+h,-size);
    glEnd();
}

void drawHole(const LevelInfo& lvl, float y)
{
    glColor4f(0.0f,0.0f,0.0f,1.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(lvl.holePos.x,y+0.001f,lvl.holePos.y);
    for(auto &v: lvl.crackVerts){
        glVertex3f(v.x,y+0.001f,v.y);
    }
    glEnd();
    // crack lines
    glBegin(GL_LINES);
    glColor4f(0.1f,0.1f,0.1f,1.0f);
    std::uniform_real_distribution<float> dist(0.5f,1.5f);
    for(int i=0;i<10;i++){
        float ang = (float)i/10*2*M_PI;
        float len = 0.6f*dist(rng);
        glVertex3f(lvl.holePos.x,y+0.002f,lvl.holePos.y);
        glVertex3f(lvl.holePos.x + cosf(ang)*len, y+0.002f, lvl.holePos.y + sinf(ang)*len);
    }
    glEnd();
}

void saveScreenshot(const char* filename, int w, int h)
{
    std::vector<unsigned char> data(w*h*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,data.data());
    // flip vertically
    std::vector<unsigned char> flipped(w*h*3);
    for(int y=0;y<h;y++)
        memcpy(&flipped[y*w*3], &data[(h-1-y)*w*3], w*3);
    stbi_write_png(filename,w,h,3,flipped.data(),w*3);
}

int main(int argc, char** argv)
{
    int step = 3;
    bool autoMove = false;
    if (argc>1) step = atoi(argv[1]);
    if (argc>2) autoMove = true;
    const int screenW=800, screenH=800;
    if(!glfwInit()) return -1;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // headless window
    GLFWwindow* win = glfwCreateWindow(screenW,screenH,"Glass Ball",NULL,NULL);
    if(!win){ glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    // setup camera
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)screenW/screenH, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);

    Vec3 ball{0,0.5f,0};
    Vec3 vel{0,0,0};
    float ballRadius=0.5f;
    float friction=0.98f;
    float gravity=-9.8f;
    float planeSpacing=4.0f;
    int currentLevel=0;

    LevelInfo curLvl = getLevel(0);
    LevelInfo nextLvl = getLevel(1);

    bool falling=false;
    int frame=0;
    while(!glfwWindowShouldClose(win)){
        glfwPollEvents();
        if(step>=2){
            if(glfwGetKey(win,GLFW_KEY_LEFT)==GLFW_PRESS) vel.x -= 5*0.016f;
            if(glfwGetKey(win,GLFW_KEY_RIGHT)==GLFW_PRESS) vel.x += 5*0.016f;
            if(glfwGetKey(win,GLFW_KEY_UP)==GLFW_PRESS) vel.z -= 5*0.016f;
            if(glfwGetKey(win,GLFW_KEY_DOWN)==GLFW_PRESS) vel.z += 5*0.016f;
        }
        if(autoMove){
            if(step==2){ vel.x += 2*0.016f; }
            if(step==3){
                Vec2 hole = curLvl.holePos;
                float dx=hole.x-ball.x; float dz=hole.y-ball.z;
                float len=sqrtf(dx*dx+dz*dz);
                if(len>0.1f) { vel.x += dx/len*2*0.016f; vel.z += dz/len*2*0.016f; }
            }
        }
        if(step>=2){
            vel.x*=friction; vel.z*=friction;
            ball.x+=vel.x*0.016f;
            ball.z+=vel.z*0.016f;
            float limit=8.5f; // wall limit
            if(ball.x<-limit){ ball.x=-limit; vel.x=0; }
            if(ball.x>limit){ ball.x=limit; vel.x=0; }
            if(ball.z<-limit){ ball.z=-limit; vel.z=0; }
            if(ball.z>limit){ ball.z=limit; vel.z=0; }
        }
        if(step==3){
            // check hole
            float dx=ball.x-curLvl.holePos.x;
            float dz=ball.z-curLvl.holePos.y;
            if(!falling && dx*dx+dz*dz < 0.25f){ falling=true; vel.y=-1.0f; }
            if(falling){
                vel.y += gravity*0.016f;
                ball.y += vel.y*0.016f;
                float nextY = -(currentLevel+1)*planeSpacing + ballRadius;
                if(ball.y <= nextY){
                    ball.y=nextY;
                    vel.y=-vel.y*0.3f;
                    if(fabs(vel.y)<0.5f){
                        falling=false; vel.y=0; currentLevel++; curLvl=nextLvl; nextLvl=getLevel(currentLevel+1);
                        ball.y = -(currentLevel)*planeSpacing + ballRadius;
                    }
                }
            }
        }

        // camera
        float camY = ball.y + 10.0f;
        glLoadIdentity();
        gluLookAt(ball.x, camY, ball.z, ball.x, ball.y, ball.z, 0,0,-1);

        glClearColor(0.5f,0.6f,0.7f,1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // draw planes
        glColor4f(0.7f,0.8f,0.9f,0.7f);
        drawPlane(-(currentLevel)*planeSpacing);
        drawWalls(-(currentLevel)*planeSpacing);
        if(step==3){
            drawHole(curLvl, -(currentLevel)*planeSpacing);
            drawPlane(-(currentLevel+1)*planeSpacing);
            drawWalls(-(currentLevel+1)*planeSpacing);
            drawHole(nextLvl, -(currentLevel+1)*planeSpacing);
        }

        // draw ball
        glPushMatrix();
        glTranslatef(ball.x, ball.y, ball.z);
        glColor3f(0.7f,0.7f,0.7f);
        GLUquadric* quad = gluNewQuadric();
        gluSphere(quad, ballRadius, 32,32);
        gluDeleteQuadric(quad);
        glPopMatrix();

        glfwSwapBuffers(win);

        frame++;
        if(frame==120) saveScreenshot(step==1?"screenshot_step1.png":step==2?"screenshot_step2.png":"screenshot_step3.png", screenW, screenH);
        if(frame>180) break; // run few frames
    }
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
