/*
  Copyright (c) 2017 Alberto Otero de la Roza
  <aoterodelaroza@gmail.com>.

  imgui-goodies is free software: you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  imgui-goodies is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_dock.h>
#include <imgui_widgets.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;
using namespace ImGui;

static void error_callback(int error, const char* description){
  fprintf(stderr, "Error %d: %s\n", error, description);
}

int main(int argc, char *argv[]){
  // Initialize
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) exit(EXIT_FAILURE);

  // Set up window
  GLFWwindow* rootwin = glfwCreateWindow(1280, 720, "gcritic2", nullptr, nullptr);
  assert(rootwin!=nullptr);
  glfwMakeContextCurrent(rootwin);

  // Setup ImGui binding
  ImGui_ImplGlfwGL2_Init(rootwin, true);

  // GUI settings
  ImGuiIO& io = GetIO();
  io.IniFilename = nullptr;

  // Main loop
  while (!glfwWindowShouldClose(rootwin)){
    // New frame
    glfwPollEvents();
    ImGui_ImplGlfwGL2_NewFrame();
    ImGuiContext *g = GetCurrentContext();
    static bool first = true;

    // Main menu bar
    if (BeginMainMenuBar()){
      if (BeginMenu("File")){
        if (MenuItem("Quit","Ctrl+Q"))
          glfwSetWindowShouldClose(rootwin, GLFW_TRUE);
        EndMenu();
      }
      SameLine(0, GetWindowSize().x-250.);
      Text("%.3f ms/frame (%.1f FPS)", 1000.0f / GetIO().Framerate, GetIO().Framerate);
    }
    EndMainMenuBar();

    static bool pcont = true;
    ImGui::SetNextWindowPos(ImVec2(270,20),ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300,300),ImGuiSetCond_FirstUseEver);
    Dock *dcont1 = nullptr;
    if (pcont)
      dcont1 = ImGui::Container("Floating Container",&pcont);

    ImGui::SetNextWindowPos(ImVec2(270,370),ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300,300),ImGuiSetCond_FirstUseEver);
    Dock *dcont2 = ImGui::Container("Fixed Container",nullptr,ImGuiWindowFlags_NoResize|
                                    ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Some docks unattached on the side
    static bool popen[10] = {true,true,true,true,true,true,true,true,true,true};
    for (int i=0;i<10;i++){
      if (popen[i]){
        SetNextWindowPos(ImVec2(20.f,40.f),ImGuiSetCond_FirstUseEver);
        SetNextWindowSize(ImVec2(200.f,200.f),ImGuiSetCond_FirstUseEver);
        char tmp[16];
        sprintf(tmp,"Dock #%d",i);
        if (BeginDock(tmp,&(popen[i]))){
          Text("Hello, world!");
          if (Button("Click me!")){
            printf("Clicked in dock %d\n",i);
          }
        }
        EndDock();
      }
    }
    
    // Some docks attached to a container
    static Dock *pwhich[5] = {dcont1,dcont2,dcont1,dcont2,dcont1};
    static bool popen2[3] = {true,true,true};
    for (int i=0;i<3;i++){
      if (popen2[i]){
        char tmp[16];
        sprintf(tmp,"Dck #%d",i);
        if (BeginDock(tmp,&(popen2[i]),0,0,pwhich[i])){
          Text("Hello, world!");
          if (Button("Click me!")){
            printf("Clicked in dock %d\n",i);
          }
        }
        Dock *dtmp = GetCurrentDock();
        EndDock();
        if (first)
          dtmp->setDetachedDockSize(100.f, 100.f);
      }
    }
    for (int i=3;i<5;i++){
      char tmp[16];
      sprintf(tmp,"Dck #%d",i);
      if (BeginDock(tmp,nullptr,0,0,pwhich[i])){
        Text("You can not close me.");
        if (Button("Click me!")){
          printf("Clicked in dock %d\n",i);
        }
      }
      Dock *dtmp = GetCurrentDock();
      EndDock();
      if (first)
        dtmp->setDetachedDockSize(100.f, 100.f);
    }

    // Draw the current scene
    int w, h;
    glfwGetFramebufferSize(rootwin,&w,&h);
    glViewport(0,0,w,h);
    glClearColor(1.0f,0.8f,0.5f,0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render and swap
    Render();
    glfwSwapBuffers(rootwin);
    first = false;
  }

  // Cleanup
  ShutdownDock();
  ImGui_ImplGlfwGL2_Shutdown();
  glfwTerminate();

  return 0;
}

