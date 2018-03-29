/*
  Copyright (c) 2017 Alberto Otero de la Roza
  <aoterodelaroza@gmail.com>, Robin Myhr <x@example.com>, Isaac
  Visintainer <x@example.com>, Richard Greaves <x@example.com>, Ángel
  Martín Pendás <angel@fluor.quimica.uniovi.es> and Víctor Luaña
  <victor@fluor.quimica.uniovi.es>.

  critic2 is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at
  your option) any later version.

  critic2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// Rewritten from: git@github.com:vassvik/imgui_docking_minimal.git
// Original code by vassvik (?) released as public domain.
// See header file (imgui_dock.h) for instructions.

#include "imgui.h"
#define IMGUI_DEFINE_PLACEMENT_NEW
#include "imgui_internal.h"
#include "imgui_dock.h"
#include "imgui_widgets.h"
#include <imgui_impl_glfw.h>
#include <unordered_map>
#include <algorithm>
#include <string>

using namespace ImGui;

// Dock context declarations
static Dock *currentdock = nullptr; // currently open dock (between BeginDock and EndDock)
static unordered_map<string,Dock*> dockht = {}; // global dock hash table (string key)
static unordered_map<ImGuiWindow*,Dock*> dockwin = {}; // global dock hash table (window key)
static Dock *FindHoveredDock(int type = -1); // find the container hovered by the mouse
static void placeWindow(ImGuiWindow* base,ImGuiWindow* moved,int idelta); // place a window relative to another in the window stack
static void killDock(Dock *dd); // erase the dock from the context and delete the object

//xx// Dock context methods //xx//

static Dock *FindHoveredDock(int type){
  ImGuiContext *g = GetCurrentContext();
  for (int i = g->Windows.Size-1; i >= 0; i--){
    ImGuiWindow *window = g->Windows[i];
    if (window->Flags & ImGuiWindowFlags_NoInputs)
      continue;
    if (window->Flags & ImGuiWindowFlags_ChildWindow)
      continue;
    ImRect bb(window->WindowRectClipped.Min - g->Style.TouchExtraPadding, window->WindowRectClipped.Max + g->Style.TouchExtraPadding);
    if (!bb.Contains(g->IO.MousePos))
      continue;
    Dock *dock = dockwin[window];
    if (!dock)
      if (!(window->WasActive)) // this window is not on the screen
        continue;
      else
        return nullptr;
    if (!dock->hoverable || dock->hidden || dock->status == Dock::Status_Closed)
      continue;
    if (dock->collapsed)
      return nullptr;
    if (type >= 0 && dock->type != type)
      return nullptr;
    return dock;
  }
  return nullptr;
}

static void placeWindow(ImGuiWindow* base,ImGuiWindow* moved,int idelta){
  ImGuiContext *g = GetCurrentContext();

  if (!base || !moved) return;

  int ibase = -1, imoved = -1;
  for (int i = 0; i < g->Windows.Size; i++){
    if (g->Windows[i] == base)
      ibase = i;
    if (g->Windows[i] == moved)
      imoved = i;
  }
  if (ibase < 0 || imoved < 0 || imoved == ibase + idelta) return;
  g->Windows.erase(g->Windows.begin() + imoved);
  if (imoved < ibase) ibase--;
  if (ibase + idelta > g->Windows.size())
    g->Windows.push_back(moved);
  else
    g->Windows.insert(g->Windows.begin() + max(ibase + idelta,0),moved);
}

static void killDock(Dock *dd){
  ImGuiContext *g = GetCurrentContext();

  dockht.erase(string(dd->label));
  if (dd->window){
    if (dd->automatic){
      int i = 0;
      while (i < g->Windows.Size){
        if (g->Windows[i] == dd->window){
          g->Windows.erase(g->Windows.begin() + i);
          continue;
        }
        if (dd->tabwin && g->Windows[i] == dd->tabwin){
          g->Windows.erase(g->Windows.begin() + i);
          continue;
        }
        i++;
      }
    }
    dockwin.erase(dd->window);
  }
  delete dd;
}

//xx// Dock methods //xx//

bool Dock::IsMouseHoveringTabBar(){
  const float ycush = 0.5 * ImGuiStyleWidgets.TabHeight;
  const ImVec2 ytabcushiondn = ImVec2(0.f,ycush);
  const ImVec2 ytabcushionup = ImVec2(0.f,this->status==Dock::Status_Docked?0.:ycush);
  return !this->stack.empty() && IsMouseHoveringRect(this->tabbarrect.Min-ytabcushionup,this->tabbarrect.Max+ytabcushiondn,false);
}

Dock::Drop_ Dock::IsMouseHoveringEdge(){
  // 1:top, 2:right, 3:bottom, 4:left
  const float dx = ImGuiStyleWidgets.DropTargetLooseness;
  const float minedge = ImGuiStyleWidgets.DropTargetMinsizeEdge;
  const float maxedge = ImGuiStyleWidgets.DropTargetMaxsizeEdge;
  const float edgefraction = ImGuiStyleWidgets.DropTargetEdgeFraction;

  ImVec2 xmin, xmax, pts[4];
  ImVec2 pos0 = this->pos;
  pos0.y += this->window->TitleBarHeight();
  ImVec2 size = this->size;
  size.y -= this->window->TitleBarHeight();
  float aside = fmin(fmax(edgefraction * fmin(size.x,size.y),maxedge),minedge);

  // 1: top
  xmin = pos0;
  xmin.x += dx;
  xmax.x = pos0.x + size.x - dx;
  xmax.y = xmin.y + aside;
  pts[0] = xmin;
  pts[1] = {xmin.x + aside, xmax.y};
  pts[2] = {xmax.x - aside, xmax.y};
  pts[3] = {xmax.x, xmin.y};
  if (IsMouseHoveringConvexPoly(pts,4))
    return Drop_Top;

  // 2: right
  xmin.x = pos0.x + size.x - aside;
  xmin.y = pos0.y + dx;
  xmax = pos0 + size;
  xmax.y -= dx;
  pts[0] = {xmax.x,xmin.y};
  pts[1] = {xmin.x,xmin.y + aside};
  pts[2] = {xmin.x,xmax.y - aside};
  pts[3] = xmax;
  if (IsMouseHoveringConvexPoly(pts,4))
    return Drop_Right;

  // 3: bottom
  xmax = pos0 + size;
  xmax.x -= dx;
  xmin.x = pos0.x + dx;
  xmin.y = xmax.y - aside;
  pts[0] = {xmin.x,xmax.y};
  pts[1] = {xmin.x + aside,xmin.y};
  pts[2] = {xmax.x - aside,xmin.y};
  pts[3] = xmax;
  if (IsMouseHoveringConvexPoly(pts,4))
    return Drop_Bottom;

  // 4: left
  xmin = pos0;
  xmin.y += dx;
  xmax.x = xmin.x + aside;
  xmax.y = pos0.y + size.y - dx;
  pts[0] = xmin;
  pts[1] = {xmax.x, xmin.y + aside};
  pts[2] = {xmax.x, xmax.y - aside};
  pts[3] = {xmin.x,xmax.y};
  if (IsMouseHoveringConvexPoly(pts,4))
    return Drop_Left;

  return Drop_None;
}

bool Dock::IsMouseHoveringFull(){
  ImVec2 a, b;
  ImVec2 pos0 = this->pos;
  pos0.y += this->window->TitleBarHeight();
  ImVec2 size = this->size;
  size.y -= this->window->TitleBarHeight();
  float aside = ImGuiStyleWidgets.DropTargetFullFraction * fmin(size.x,size.y);
  a.x = pos0.x + 0.5f * size.x - 0.5f * aside;
  a.y = pos0.y + 0.5f * size.y - 0.5f * aside;
  b = a + ImVec2(aside,aside);
  return IsMouseHoveringRect(a,b,false);
}

int Dock::getNearestTabBorder(){
  if (!this->IsMouseHoveringTabBar()) return -1;
  int ithis = this->tabsx.size()-1;
  float xpos = GetMousePos().x;
  for (int i = 0; i < this->tabsx.size(); i++){
    if (xpos < this->tabsx[i]){
      ithis = i;
      break;
    }
  }
  if (ithis > 0 && (xpos-this->tabsx[ithis-1]) < (this->tabsx[ithis]-xpos))
    ithis = ithis - 1;
  return ithis;
}

void Dock::showDropTargetFull(){
  SetNextWindowSize(ImVec2(0,0));
  ImU32 color = GetColorU32(ImGuiStyleWidgets.Colors[ImGuiColWidgets_DropTarget]);
  ImU32 coloractive = GetColorU32(ImGuiStyleWidgets.Colors[ImGuiColWidgets_DropTargetActive]);
  ImVec2 pos0 = this->pos;
  pos0.y += this->window->TitleBarHeight();
  ImVec2 size = this->size;
  size.y -= this->window->TitleBarHeight();

  Begin("##Drop",nullptr,ImGuiWindowFlags_Tooltip|ImGuiWindowFlags_NoTitleBar|
        ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize);
  ImDrawList* drawl = GetWindowDrawList();
  drawl->PushClipRectFullScreen();

  ImVec2 a, b;
  float aside = ImGuiStyleWidgets.DropTargetFullFraction * fmin(size.x,size.y);
  a.x = pos0.x + 0.5f * size.x - 0.5f * aside;
  a.y = pos0.y + 0.5f * size.y - 0.5f * aside;
  b = a + ImVec2(aside,aside);
  
  if (IsMouseHoveringRect(a,b,false))
    drawl->AddRectFilled(a, b, coloractive, GetStyle().WindowRounding);
  else
    drawl->AddRectFilled(a, b, color, GetStyle().WindowRounding);
  drawl->PopClipRect();
  End();
}

void Dock::showDropTargetOnTabBar(){
  ImGuiContext *g = GetCurrentContext();
  const float triside = g->FontSize;

  int ithis = this->getNearestTabBorder();

  SetNextWindowSize(ImVec2(0,0));
  Begin("##Drop",nullptr,ImGuiWindowFlags_Tooltip|ImGuiWindowFlags_NoTitleBar|
        ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize);
  ImDrawList* drawl = GetWindowDrawList();
  drawl->PushClipRectFullScreen();
  ImU32 docked_color = GetColorU32(ImGuiStyleWidgets.Colors[ImGuiColWidgets_DropTargetActive]);

  ImVec2 a, b, c;
  // a.x = this->tabsx[ithis]; a.y = this->tabbarrect.Max.y;
  // b.x = a.x + 0.5 * triside; b.y = a.y + triside * sqrt(3.)/2.;
  // c.x = a.x - 0.5 * triside; c.y = b.y;
  // drawl->AddTriangleFilled(a,b,c,docked_color);
  // a.y = this->tabbarrect.Min.y;
  // b.y = a.y - triside * sqrt(3.)/2.;
  // c.y = b.y;
  // drawl->AddTriangleFilled(a,b,c,docked_color);
  a.x = this->tabsx[ithis] - 0.5 * triside;
  a.y = this->tabbarrect.Min.y - 0.5 * triside;
  b.x = this->tabsx[ithis] + 0.5 * triside;
  b.y = this->tabbarrect.Max.y + 0.5 * triside;
  drawl->AddRectFilled(a,b,docked_color);

  drawl->PopClipRect();
  End();
}

void Dock::showDropTargetEdge(Drop_ edge, bool active){
  // 1:top, 2:right, 3:bottom, 4:left
  ImU32 color = GetColorU32(ImGuiStyleWidgets.Colors[ImGuiColWidgets_DropTarget]);
  ImU32 coloractive = GetColorU32(ImGuiStyleWidgets.Colors[ImGuiColWidgets_DropTargetActive]);
  const float dx = ImGuiStyleWidgets.DropTargetLooseness;
  const float minedge = ImGuiStyleWidgets.DropTargetMinsizeEdge;
  const float maxedge = ImGuiStyleWidgets.DropTargetMaxsizeEdge;
  const float edgefraction = ImGuiStyleWidgets.DropTargetEdgeFraction;

  if (edge > 0){
    ImGuiContext *g = GetCurrentContext();
    ImVec2 pos0 = this->pos;
    pos0.y += this->window->TitleBarHeight();
    ImVec2 size = this->size;
    size.y -= this->window->TitleBarHeight();
    float aside = fmin(fmax(edgefraction * fmin(size.x,size.y),maxedge),minedge);

    ImVec2 p0, p1, p2, p3, xmin, xmax;
    if (edge == Drop_Top) {
      xmin = pos0;
      xmin.x += dx;
      xmax.x = pos0.x + size.x - dx;
      xmax.y = xmin.y + aside;
      p0 = xmin;
      p1 = {xmin.x + aside, xmax.y};
      p2 = {xmax.x - aside, xmax.y};
      p3 = {xmax.x, xmin.y};
    } else if (edge == Drop_Right) { 
      xmin.x = pos0.x + size.x - aside;
      xmin.y = pos0.y + dx;
      xmax = pos0 + size;
      xmax.y -= dx;
      p0 = {xmax.x,xmin.y};
      p1 = {xmin.x,xmin.y + aside};
      p2 = {xmin.x,xmax.y - aside};
      p3 = xmax;
    } else if (edge == Drop_Bottom) { 
      xmax = pos0 + size;
      xmax.x -= dx;
      xmin.x = pos0.x + dx;
      xmin.y = xmax.y - aside;
      p0 = {xmin.x,xmax.y};
      p1 = {xmin.x + aside,xmin.y};
      p2 = {xmax.x - aside,xmin.y};
      p3 = xmax;
    } else if (edge == Drop_Left) { 
      xmin = pos0;
      xmin.y += dx;
      xmax.x = xmin.x + aside;
      xmax.y = pos0.y + size.y - dx;
      p0 = xmin;
      p1 = {xmax.x, xmin.y + aside};
      p2 = {xmax.x, xmax.y - aside};
      p3 = {xmin.x,xmax.y};
    }

    SetNextWindowSize(ImVec2(0,0));
    Begin("##Drop",nullptr,ImGuiWindowFlags_Tooltip|ImGuiWindowFlags_NoTitleBar|
    	  ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize);
    ImDrawList* drawl = GetWindowDrawList();
    drawl->PushClipRectFullScreen();
    ImVec2 pts[4] = {p0, p1, p2, p3};
    drawl->AddConvexPolyFilled(pts, 4, active?coloractive:color, g->Style.AntiAliasedShapes);
    drawl->PopClipRect();
    End();
  }

  if (active){
    for (int i = 1; i < 5; i++)
      if (edge != (Drop_) i)
	this->showDropTargetEdge((Drop_) i,false);
  }
}

int Dock::OpStack_Find(Dock *dthis){
  int n = -1;
  for (auto dd : this->stack) {
    n++;
    if (dd == dthis)
      return n;
  }
  return -1;
}

void Dock::OpStack_Insert(Dock *dnew, int ithis /*=-1*/){
  if (ithis < 0 || ithis >= this->stack.size())
    this->stack.push_back(dnew);
  else{
    int n = -1;
    for (auto it = this->stack.begin(); it != this->stack.end(); ++it){
      n++;
      if (n == ithis){
        this->stack.insert(it,dnew);
        break;
      }
    }
  }
  dnew->root = this->root;
  dnew->parent = this;
}

void Dock::OpStack_Replace(Dock *replaced, Dock* replacement, bool erase){
  for(auto it = this->stack.begin(); it != this->stack.end(); it++){
    if (*it == replaced){
      this->stack.insert(this->stack.erase(it),replacement);
      replacement->parent = this;
      replacement->root = this->root;
      replaced->parent = nullptr;
      replaced->root = nullptr;
      break;
    }
  }
  if (erase)
    killDock(replaced);
}

void Dock::OpStack_Remove(Dock *dd, bool erase){
  int n = 0;
  for(auto it = this->stack.begin(); it != this->stack.end(); it++){
    ++n;
    if (*it == dd){
      this->stack.erase(it);
      dd->parent = nullptr;
      dd->root = nullptr;
      break;
    }
  }

  // erase the sliding bar
  int m = -1;
  for (auto it = this->tabsx.begin(); it != this->tabsx.end(); it++){
    ++m;
    if (m == 0 || m == tabsx.size()-1)
      continue;
    if ((dd->splithint == 1 && m == n) || 
	(dd->splithint == -1 && m == n-1) ||
	(dd->splithint == 0 && (m == n-1 || m == n))){
      this->tabsx.erase(it);
      break;
    }
  }

  // reset the split hint for the tab on the other side
  if (dd->splithint != 0){
    int m = 0;
    for(auto it = this->stack.begin(); it != this->stack.end(); it++){
      ++m;
      if ((n == m && dd->splithint == 1) || (n-1 == m && dd->splithint == -1)){
	(*it)->splithint = 0;
	break;
      }
    }
  }

  // kill the dock
  if (erase)
    killDock(dd);
}

Dock *Dock::OpRoot_ReplaceHV(Dock::Type_ type,bool before,Dock *dcont/*=nullptr*/,ImVec2 weight/*={1.f,1.f}*/){
  // 1:top, 2:right, 3:bottom, 4:left
  Dock *dpar = this->parent;
  Dock *root = dpar->root;
  if (!dcont){
    // new empty container
    char label1[strlen(root->label)+15];
    ImFormatString(label1,IM_ARRAYSIZE(label1),"%s__%d__",root->label,++(root->nchild_));
    dcont = new Dock;
    IM_ASSERT(dcont);
    dcont->label = ImStrdup(label1);
    dockht[string(dcont->label)] = dcont;
    dcont->type = Dock::Type_Container;
    dcont->status = Dock::Status_Docked;
    dcont->hoverable = true;
    dcont->automatic = true;
    dcont->dockflags = root->dockflags;
    dcont->splitweight = weight;
  }
  root->nchild++;

  // new horizontal or vertical container
  char label2[strlen(root->label)+15];
  ImFormatString(label2,IM_ARRAYSIZE(label2),"%s__%d__",root->label,++(root->nchild_));
  Dock *dhv = new Dock;
  IM_ASSERT(dhv);
  dhv->label = ImStrdup(label2);
  dockht[string(dhv->label)] = dhv;
  dhv->type = type;
  dhv->status = Dock::Status_Docked;
  dhv->hoverable = false;
  dhv->automatic = true;
  dhv->dockflags = root->dockflags;
  root->nchild++;

  // build the new horizontal/vertical
  if (before){
    dhv->stack.push_back(dcont);
    dhv->stack.push_back(this);
  } else {
    dhv->stack.push_back(this);
    dhv->stack.push_back(dcont);
  }

  // set the ratio based on the weights
  float ratio;
  if (before)
    if (type == Dock::Type_Vertical)
      ratio = dcont->splitweight.x / (this->splitweight.x+dcont->splitweight.x);
    else
      ratio = dcont->splitweight.y / (this->splitweight.y+dcont->splitweight.y);
  else
    if (type == Dock::Type_Vertical)
      ratio = this->splitweight.x / (this->splitweight.x+dcont->splitweight.x);
    else
      ratio = this->splitweight.y / (this->splitweight.y+dcont->splitweight.y);

  // fill the sliding bar positions
  dhv->tabsx.clear();
  dhv->tabsx.push_back(0.0f);
  dhv->tabsx.push_back(ratio);
  dhv->tabsx.push_back(1.0f);

  // replace this with the new horizontal/vertical container in the parent's stack
  dpar->OpStack_Replace(this,dhv,false);

  // rearrange the parent and root variables
  dhv->root = root;
  dcont->root = root;
  this->root = root;
  dhv->parent = dpar;
  this->parent = dhv;
  dcont->parent = dhv;

  // return the new container
  return dcont;
}

Dock *Dock::OpRoot_AddToHV(bool before,Dock *dcont/*=nullptr*/,ImVec2 weight/*={1.f,1.f}*/){
  // 1:top, 2:right, 3:bottom, 4:left
  Dock *dpar = this->parent;
  Dock *root = dpar->root;
  if (!dcont){
    // new empty container
    char label1[strlen(root->label)+15];
    ImFormatString(label1,IM_ARRAYSIZE(label1),"%s__%d__",root->label,++(root->nchild_));
    dcont = new Dock;
    IM_ASSERT(dcont);
    dcont->label = ImStrdup(label1);
    dockht[string(dcont->label)] = dcont;
    dcont->type = Dock::Type_Container;
    dcont->status = Dock::Status_Docked;
    dcont->hoverable = true;
    dcont->automatic = true;
    dcont->dockflags = root->dockflags;
    dcont->splitweight = weight;
  }
  root->nchild++;

  // add to the parent's stack
  int n = 0;
  for(auto it = dpar->stack.begin(); it != dpar->stack.end(); it++){
    ++n;
    if (*it == this){
      if (before)
        dpar->stack.insert(it,dcont);
      else
        dpar->stack.insert(++it,dcont);
      break;
    }
  }

  // the new tab splits the old tab in half
  int m = -1;
  for (auto it = dpar->tabsx.begin(); it != dpar->tabsx.end(); it++){
    m++;
    if (n == m){
      dpar->tabsx.insert(it,-1.f);
      float ratio;
      if (before)
	if (dpar->type == Dock::Type_Vertical)
	  ratio = dcont->splitweight.x / (this->splitweight.x+dcont->splitweight.x);
	else
	  ratio = dcont->splitweight.y / (this->splitweight.y+dcont->splitweight.y);
      else
	if (dpar->type == Dock::Type_Vertical)
	  ratio = this->splitweight.x / (this->splitweight.x+dcont->splitweight.x);
	else
	  ratio = this->splitweight.y / (this->splitweight.y+dcont->splitweight.y);

      dpar->tabsx[n] = dpar->tabsx[n-1] + ratio * (dpar->tabsx[n+1] - dpar->tabsx[n-1]);
      dcont->splithint = (before?1:-1);
      this->splithint = - dcont->splithint;
      break;
    }
  }

  // rearrange the parent and root variables
  dcont->root = root;
  dcont->parent = dpar;

  // return the new container
  return dcont;
}

void Dock::OpRoot_FillEmpty(){
  if (!this->stack.empty() || this->type != Dock::Type_Root) return;

  this->nchild = 1;
  char tmp[strlen(this->label)+15];
  ImFormatString(tmp,IM_ARRAYSIZE(tmp),"%s__%d__",this->label,++(this->nchild_));
  Dock *dcont = new Dock;
  IM_ASSERT(dcont);
  dcont->label = ImStrdup(tmp);
  dockht[string(dcont->label)] = dcont;
  dcont->type = Dock::Type_Container;
  dcont->status = Dock::Status_Docked;
  dcont->hoverable = true;
  dcont->automatic = true;
  dcont->dockflags = this->dockflags;
  dcont->parent = this;
  dcont->root = this;
  this->stack.push_back(dcont);
}

void Dock::raiseDock(){
  ImGuiContext *g = GetCurrentContext();
  if (!this->window) return;

  for (int i = 0; i < g->Windows.Size; i++)
    if (g->Windows[i] == this->window){
      g->Windows.erase(g->Windows.begin() + i);
      break;
    }
  g->Windows.push_back(this->window);
}

void Dock::raiseOrSinkDock(){
  ImGuiContext *g = GetCurrentContext();
  if (!this->window) return;

  for (int i = 0; i < g->Windows.Size; i++)
    if (g->Windows[i] == this->window){
      g->Windows.erase(g->Windows.begin() + i);
      break;
    }
  if ((this->flags & ImGuiWindowFlags_NoBringToFrontOnFocus) &&
      g->Windows.front() != this->window)
    g->Windows.insert(g->Windows.begin(),this->window);
  else 
    g->Windows.push_back(this->window);
}

void Dock::focusContainer(){
  ImGuiContext *g = GetCurrentContext();

  bool raise = true;
  if (this->root)
    raise = !(this->root->flags & ImGuiWindowFlags_NoBringToFrontOnFocus);
  else
    raise = !(this->flags & ImGuiWindowFlags_NoBringToFrontOnFocus);

  // Push the container and the docked window to the top of the stack
  if (raise){
    if (this->root)
      this->root->raiseDock();
    this->raiseDock();
    if (this->currenttab)
      this->currenttab->raiseDock();
  }

  // The docked window becomes focused, if possible. Otherwise, the container
  if (!this->currenttab){
    g->HoveredRootWindow = this->window;
    g->HoveredWindow = this->window;
  } else {
    g->HoveredRootWindow = this->currenttab->window;
    g->HoveredWindow = this->currenttab->window;
    if (g->ActiveIdWindow == this->currenttab->window && !g->ActiveIdIsAlive)
      ClearActiveID();
  }

  // The container (or the root container, if available) is being moved
  if (!IsAnyItemActive() && !IsAnyItemHovered() && g->IO.MouseClicked[0]){
    if (this->root){
      g->MovingWindow = this->root->window;
      g->MovingWindowMoveId = this->root->window->RootWindow->MoveId;
    } else {
      g->MovingWindow = this->window;
      g->MovingWindowMoveId = this->window->RootWindow->MoveId;
    }
    if (this->currenttab)
      SetActiveID(g->MovingWindowMoveId, this->currenttab->window->RootWindow);
    else
      SetActiveID(g->MovingWindowMoveId, this->window->RootWindow);
  }
}

void Dock::liftContainer(){
  ImGuiContext *g = GetCurrentContext();

  Dock *dpar = this->parent;
  Dock *droot = this->root;

  dpar->OpStack_Remove(this,false);
  droot->nchild--;
  dpar->killContainerMaybe();
  this->unDock();
  this->status = Dock::Status_Dragged;
  this->hoverable = false;
  this->pos = GetMousePos() - ImVec2(0.5*this->size.x,min(ImGuiStyleWidgets.TabHeight,0.2f*this->size.y));
  ClearActiveID();
  g->MovingWindow = this->window;
  g->MovingWindowMoveId = this->window->RootWindow->MoveId;
  if (this->currenttab)
    SetActiveID(g->MovingWindowMoveId, this->currenttab->window->RootWindow);
  else
    SetActiveID(g->MovingWindowMoveId, this->window->RootWindow);
}

void Dock::newDock(Dock *dnew, int ithis /*=-1*/){
  if (!(this->type == Dock::Type_Container)) return;

  dnew->status = Dock::Status_Docked;
  dnew->hoverable = false;
  this->currenttab = dnew;
  this->splitweight = dnew->splitweight;
  this->OpStack_Insert(dnew,ithis);
}

Dock *Dock::newDockRoot(Dock *dnew, Drop_ iedge){
  // 1:top, 2:right, 3:bottom, 4:left
  if (iedge == Drop_None) return nullptr;
  Dock *dcont = nullptr;
  if (dnew->type == Dock::Type_Container)
    dcont = dnew;

  if (this->type == Dock::Type_Root){
    dcont = this->stack.back()->newDockRoot(dnew,iedge);
  } else if (this->type == Dock::Type_Horizontal || this->type == Dock::Type_Vertical){
    dcont = this->stack.back()->newDockRoot(dnew,iedge);
  } else {
    if (this->parent->type == Dock::Type_Root){
      Type_ type;
      if (iedge == Drop_Top || iedge == Drop_Bottom){
        type = Dock::Type_Horizontal;
        dcont = this->OpRoot_ReplaceHV(type,iedge==Drop_Top||iedge==Drop_Left,dcont,dnew->splitweight);
      } else if (iedge == Drop_Right || iedge == Drop_Left){
        type = Dock::Type_Vertical;
        dcont = this->OpRoot_ReplaceHV(type,iedge==Drop_Top||iedge==Drop_Left,dcont,dnew->splitweight);
      } else {
        if (this->automatic)
          if (dnew->type == Type_Container)
            this->parent->OpStack_Replace(this,dnew,true);
          else
            dcont = this;
        else
          return nullptr;
      }
    } else if (this->parent->type == Dock::Type_Horizontal){
      if (iedge == Drop_Top || iedge == Drop_Bottom){
        dcont = this->OpRoot_AddToHV(iedge==Drop_Top,dcont,dnew->splitweight);
      } else {
        dcont = this->OpRoot_ReplaceHV(Dock::Type_Vertical,iedge==Drop_Left,dcont,dnew->splitweight);
      }
    } else if (this->parent->type == Dock::Type_Vertical){
      if (iedge == Drop_Right || iedge == Drop_Left){
        dcont = this->OpRoot_AddToHV(iedge==Drop_Left,dcont,dnew->splitweight);
      } else {
        dcont = this->OpRoot_ReplaceHV(Dock::Type_Horizontal,iedge==Drop_Top,dcont,dnew->splitweight);
      }
    }

    dnew->hoverable = (dnew->type == Dock::Type_Dock);
    if (dnew->type == Dock::Type_Dock)
      dcont->newDock(dnew);
  }
  dnew->status = Dock::Status_Docked;
  dnew->root = this->root;
  return dcont;
}

void Dock::unDock(){
  this->status = Dock::Status_Open;
  this->hoverable = true;
  this->control_window_this_frame = true;
  this->size = this->size_saved;
  this->collapsed = this->collapsed_saved;
  this->flags = this->flags_saved;
  this->pos = this->pos_saved;
  this->parent = nullptr;
  this->root = nullptr;
  this->raiseOrSinkDock();
}

void Dock::clearContainer(){
  const float increment = ImGuiStyleWidgets.CascadeIncrement;

  ImVec2 pos = this->pos;
  for (auto dd : this->stack) {
    dd->unDock();
    dd->pos = pos;
    pos = pos + ImVec2(increment,increment);
  }
  this->currenttab = nullptr;
  this->stack.clear();
}

void Dock::clearRootContainer(){
  const float increment = ImGuiStyleWidgets.CascadeIncrement;

  if (this->type == Dock::Type_Root){
    this->nchild = 0;
    for (auto dd : this->stack)
      dd->clearRootContainer();
    this->nchild = 0;
    this->stack.clear();
  } else if (this->type == Dock::Type_Horizontal || this->type == Dock::Type_Vertical) {
    for (auto dd : this->stack)
      dd->clearRootContainer();
    killDock(this);
  } else if (this->type == Dock::Type_Container) {
    if (this->automatic){
      for (auto dd : this->stack) {
        dd->unDock();
        dd->pos = this->root->pos + ImVec2(this->root->nchild * increment,this->root->nchild * increment);
        (this->root->nchild)++;
      }
      killDock(this);
    } else {
      Dock *root = this->root;
      this->unDock();
      this->pos = root->pos + ImVec2(root->nchild * increment,root->nchild * increment);
      (root->nchild)++;
    }
  }
}

void Dock::killContainerMaybe(){
  // Only kill containers, horziontals, and verticals that were automatically generated
  if (!this || !(this->automatic) || !(this->root) || 
      (this->type != Dock::Type_Container && this->type != Dock::Type_Horizontal && this->type != Dock::Type_Vertical))
    return;
  Dock *dpar = this->parent;
  if (!dpar) return;

  if (this->type == Dock::Type_Container && this->stack.empty()){
    // An empty container
    // Do not remove the last container from a root container, even if it's empty
    if (dpar->type == Dock::Type_Root && dpar->stack.size() <= 1) return;
    this->root->nchild--;
    dpar->OpStack_Remove(this,true);

    // Try to kill its parent
    dpar->killContainerMaybe();
  } else if ((this->type == Dock::Type_Vertical || this->type == Dock::Type_Horizontal) && this->stack.empty()){
    // If a horizontal or vertical is empty, turn it into a container
    this->type = Dock::Type_Container;
    // then to try kill it
    this->killContainerMaybe();
  } else if ((this->type == Dock::Type_Vertical || this->type == Dock::Type_Horizontal) && this->stack.size() == 1){
    // This vertical/horizontal container only has one window ->
    // eliminate it and connect the single window to its parent
    this->root->nchild--;
    dpar->OpStack_Replace(this,this->stack.back(),true);

    // Try to kill its parent
    dpar->killContainerMaybe();
  }
}

void Dock::drawTabBar(Dock **erased/*=nullptr*/){
  ImGuiContext *g = GetCurrentContext();
  const float tabheight = ImGuiStyleWidgets.TabHeight;
  const float maxtabwidth = ImGuiStyleWidgets.TabMaxWidth;
  ImVec4 text_color = g->Style.Colors[ImGuiCol_Text];
  text_color.w = 2.0 / g->Style.Alpha;
  bool raise = false;
  Dock *dderase = nullptr;

  // empty the list of tabs
  this->tabsx.Size = 0;

  // calculate the widths
  float tabwidth_long;
  if ((this->size.x - 2 * g->Style.WindowPadding.x) >= this->stack.size() * maxtabwidth)
    tabwidth_long = maxtabwidth;
  else
    tabwidth_long = round(this->size.x - 2 * g->Style.WindowPadding.x) / this->stack.size();

  // the tabbar with alpha = 1.0, no spacing in the x
  PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0,1.0));
  PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0,0.0));
  PushStyleColor(ImGuiCol_Text,text_color);

  // start placing the tabs
  char tmp[strlen(this->label)+15];
  ImFormatString(tmp,IM_ARRAYSIZE(tmp),"%s__tab__",this->label);
  if (BeginChild(tmp,ImVec2(this->size.x,0.f),false)){
    bool active = false, hovered = false;
    Dock *ddlast = nullptr;
    ImVec2 center, pos0, pos1, pos1s, text_size;
    for (auto dd : this->stack) {
      SameLine();
      // make the x-button, update the container info
      bool dragged, dclicked;
      if (ButtonWithX(dd->label, ImVec2(tabwidth_long, tabheight), (dd == this->currenttab),
                      dd->p_open, &dragged, &dclicked, 2.f/g->Style.Alpha)){
        this->currenttab = dd;
        dd->parent = this;
        dd->root = this->root;
        dd->focusContainer();
      }
      this->tabsx.push_back(GetItemRectMax().x-tabwidth_long);

      // lift the tab using the main button
      if (dragged){
        dd->unDock();
        dd->status = Dock::Status_Dragged;
        dd->hoverable = false;
        dd->pos = GetMousePos() - ImVec2(0.5*dd->size.x,0.f);
        goto erase_this_tab;
      }
      // double click detaches the tab / place it on top
      if (dclicked){
        dd->unDock();
        dd->pos = GetMousePos() - ImVec2(0.5*dd->size.x,0.f);
        g->HoveredRootWindow = dd->window;
        g->HoveredWindow = dd->window;
        g->IO.MouseDoubleClicked[0] = false; // prevent following docks from seeing this double click
        goto erase_this_tab;
      }
      // closed click kills the tab
      if (dd->p_open && !*(dd->p_open)){
	dd->unDock();
	dd->status = Dock::Status_Closed;
        goto erase_this_tab;
      }

      ddlast = dd;
      continue;

    erase_this_tab:
      dderase = dd;
      if (dd == this->currenttab){
        if (ddlast)
          this->currenttab = ddlast;
        else
          this->currenttab = nullptr;
      }
    } // dd in this->stack
    if (dderase){
      this->stack.remove(dderase);
      if (!this->currenttab && this->stack.size() > 0){
        this->currenttab = this->stack.front();
        this->currenttab->parent = this;
      }
    }
    // last item in the tabsx
    this->tabsx.push_back(GetItemRectMax().x);
  } // BeginChild(tmp, ImVec2(this->size.x,barheight), true)
  this->tabbarrect = GetCurrentWindowRead()->DC.LastItemRect;
  this->tabbarrect.Min.x = this->pos.x;
  this->tabwin = GetCurrentWindow();
  EndChild();
  PopStyleVar();
  PopStyleVar();
  PopStyleColor();

  if (erased) 
    if (dderase)
      *erased = this;
}

void Dock::hideTabWindow(){
  this->flags = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoScrollbar|
    ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoCollapse|
    ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs;
  this->pos = ImVec2(0.,0.);
  this->size = ImVec2(0.,0.);
  this->hidden = true;
}

void Dock::showTabWindow(Dock *dcont, bool noresize){
  if (!dcont) return;

  float topheight = max(dcont->tabbarrect.Max.y - dcont->pos.y,0.f);
  this->pos = dcont->pos;
  this->pos.y += topheight;
  this->size = dcont->size;
  this->size.y = dcont->size.y - topheight;
  this->hidden = false;
  this->flags = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|
    ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|
    ImGuiWindowFlags_NoBringToFrontOnFocus;
  if (noresize)
    this->flags = this->flags | ImGuiWindowFlags_NoResize;
  dcont->splitweight = this->splitweight;
}

void Dock::drawContainer(bool noresize, Dock **erased/*=nullptr*/){
  if (!(this->type == Dock::Type_Container)) return;

  if (this->stack.size() > 0){
    // Draw the tab
    this->drawTabBar(erased);

    // Hide all tabs
    for (auto dd : this->stack)
      dd->hideTabWindow();

    // Unhide current tab
    if (!this->hidden && !this->collapsed && this->currenttab)
      this->currenttab->showTabWindow(this,noresize);

  } else {
    this->splitweight = {1.f,1.f};
  }
}

void Dock::getMinSize(ImVec2 *minsize,ImVec2 *autosize){
  ImGuiContext *g = GetCurrentContext();
  const float barwidth = ImGuiStyleWidgets.SlidingBarWidth;

  if (minsize) *minsize = {}; 
  if (autosize) *autosize = {}; 
  if (this->type == Dock::Type_Root){
    this->stack.back()->getMinSize(minsize,autosize);
    if (autosize && this->window)
      autosize->y += this->window->TitleBarRect().GetHeight();
  } else if (this->type == Dock::Type_Horizontal) {
    ImVec2 msize_ = {}, asize_ = {};
    for (auto dd : this->stack){
      dd->getMinSize(&msize_,&asize_);
      if (minsize){
        minsize->x = max(msize_.x,minsize->x);
        minsize->y += msize_.y;
      }
      if (autosize){
        autosize->x = max(asize_.x,autosize->x);
        autosize->y += asize_.y;
      }
    }
    if (minsize)
      minsize->y += barwidth * (this->stack.size()-1);
    if (autosize)
      autosize->y += barwidth * (this->stack.size()-1);
  } else if (this->type == Dock::Type_Vertical) {
    ImVec2 msize_ = {}, asize_ = {};
    for (auto dd : this->stack){
      dd->getMinSize(&msize_,&asize_);
      if (minsize){
        minsize->x += msize_.x;
        minsize->y = max(msize_.y,minsize->y);
      }
      if (autosize){
        autosize->x += asize_.x;
        autosize->y = max(asize_.y,autosize->y);
      }
    }
    if (minsize)
      minsize->x += barwidth * (this->stack.size()-1);
    if (autosize)
      autosize->x += barwidth * (this->stack.size()-1);
  } else if (this->type == Dock::Type_Container) {
    if (minsize)
      *minsize = g->Style.WindowMinSize;
    if (autosize)
      *autosize = g->Style.WindowMinSize + g->Style.WindowPadding;
    if (this->currenttab){
      if (minsize)
        minsize->y += this->tabdz;
      if (autosize){
        autosize->x = max(autosize->x,this->currenttab->window->SizeContents.x);
        autosize->y = max(autosize->y,this->currenttab->window->SizeContents.y) + this->tabdz;
      }
    }
  } else if (this->type == Dock::Type_Dock) {
    if (minsize)
      *minsize = g->Style.WindowMinSize;
    if (autosize)
      *autosize = ImMax(g->Style.WindowMinSize + g->Style.WindowPadding,this->window->SizeContents);
  }
}

void Dock::resetRootContainerBars(){
  if (this->type == Dock::Type_Root){
    this->stack.back()->resetRootContainerBars();
  } else if (this->type == Dock::Type_Horizontal || this->type == Dock::Type_Vertical) {
    for (auto dd : this->stack)
      dd->resetRootContainerBars();

    int ntot = this->stack.size();
    if (this->tabsx.size() != ntot+1)
      this->tabsx.resize(ntot+1);
    for (int i=0;i<=ntot;i++)
      this->tabsx[i] = ((float) i) / ((float) ntot);
    for (auto it = this->stack.begin(); it != this->stack.end(); ++it)
      (*it)->splithint = 0;
  }
}

void Dock::setSlidingBarPosition(Drop_ iedge,float xpos){
  // 1:top, 2:right, 3:bottom, 4:left
  if (!this->parent) return;
  if (this->parent->type == Type_Horizontal && (iedge == Drop_Right || iedge == Drop_Left)) return;
  if (this->parent->type == Type_Vertical && (iedge == Drop_Top || iedge == Drop_Bottom)) return;
  if (iedge == Drop_None || iedge == Drop_Tab || xpos < 0.f || xpos > 1.f) return;

  int id = this->parent->OpStack_Find(this);
  int ntot = this->parent->stack.size();
  if (id == -1) return;

  if (iedge == Drop_Top || iedge == Drop_Bottom){
    if (iedge == Drop_Top && id > 0 && id < ntot)
      this->parent->tabsx[id] = xpos;
    else if (iedge == Drop_Bottom && id+1 > 0 && id+1 < ntot)
      this->parent->tabsx[id+1] = xpos;
  } else if (iedge == Drop_Right || iedge == Drop_Left){
    if (iedge == Drop_Left && id > 0 && id < ntot)
      this->parent->tabsx[id] = xpos;
    else if (iedge == Drop_Right && id+1 > 0 && id+1 < ntot)
      this->parent->tabsx[id+1] = xpos;
  }
}

void Dock::drawRootContainerBars(Dock *root){
  if (!this) return;
  ImGuiContext *g = GetCurrentContext();
  const float barwidth = ImGuiStyleWidgets.SlidingBarWidth;

  this->root = root;
  if (this->type == Dock::Type_Root){
    this->stack.back()->drawRootContainerBars(root);
  } else if (this->type == Dock::Type_Horizontal || this->type == Dock::Type_Vertical) {
    // // update the vector containing the sliding bar positions
    // int ntot = this->stack.size();
    // if (this->tabsx.size() != ntot+1)
    //   for (int i=0;i<=ntot;i++)
    //     this->tabsx[i] = ((float) i) / ((float) ntot);
      
    // draw all the sliding bars for this container
    char tmp[strlen(this->label)+15];
    float x0, x1, xmin, xmax;
    ImVec2 pos, size, mincont = {}, mincontprev;
    int direction;
    int n = -1;
    for (auto dd : this->stack){
      n++;
      mincontprev = mincont;
      dd->getMinSize(&mincont,nullptr);
      if (n != 0){
        pos = this->pos;
        size = this->size;
        if (this->type == Dock::Type_Horizontal){
          x0 = this->pos.y + (this->window?this->window->TitleBarRect().GetHeight():0.f);
          x1 = this->pos.y + this->size.y;
          xmin = x0 + this->tabsx[n-1] * (x1 - x0) + (n>1?0.5f * barwidth:0.f) + mincontprev.y;
          xmax = max(xmin,x0 + this->tabsx[n+1] * (x1 - x0) - (n<this->stack.size()-1?0.5f * barwidth:0.f) - 1.0f * barwidth - mincont.y);
          pos.y = min(xmax,max(xmin,x0 + this->tabsx[n] * (x1 - x0) - 0.5f * barwidth));
          size.y = barwidth;
          direction = 2;
        } else {
          x0 = this->pos.x;
          x1 = this->pos.x + this->size.x;
          xmin = x0 + this->tabsx[n-1] * (x1 - x0) + (n>1?0.5f * barwidth:0.f) + mincontprev.x;
          xmax = max(xmin,x0 + this->tabsx[n+1] * (x1 - x0) - (n<this->stack.size()-1?0.5f * barwidth:0.f) - 1.0f * barwidth - mincont.x);
          pos.x = min(xmax,max(xmin,x0 + this->tabsx[n] * (x1 - x0) - 0.5f * barwidth));
          size.x = barwidth;
          direction = 1;
        }
        ImFormatString(tmp,IM_ARRAYSIZE(tmp),"%s__s%d__",this->label,n);
        if (x1 > x0){
          SlidingBar(tmp, root->window, &pos, size, xmin, xmax, direction);
          if (this->type == Dock::Type_Horizontal)
            this->tabsx[n] = (pos.y + 0.5f * barwidth - x0) / (x1 - x0);
          else
            this->tabsx[n] = (pos.x + 0.5f * barwidth - x0) / (x1 - x0);
        }
      }
      dd->drawRootContainerBars(root);
    }
  }
}

void Dock::drawRootContainer(Dock *root, Dock **lift, Dock **erased, int *ncount/*=nullptr*/){
  if (!this) return;
  ImGuiContext *g = GetCurrentContext();
  const float barwidth = ImGuiStyleWidgets.SlidingBarWidth;

  this->root = root;
  if (this->type == Dock::Type_Root){
    int ncount_ = 0;
    Dock * dd = this->stack.back();
    dd->pos = this->pos;
    dd->size = this->size;
    dd->parent = this;
    dd->drawRootContainer(root,lift,erased,&ncount_);
  } else if (this->type == Dock::Type_Horizontal || this->type == Dock::Type_Vertical) {
    float x0, x1;
    float width1, width2;
    int n = -1;
    (*ncount)++;
    for (auto dd : this->stack){
      n++;
      dd->pos = this->pos;
      dd->size = this->size;
      if (this->type == Dock::Type_Horizontal){
        x0 = this->pos.y + (this->window?this->window->TitleBarRect().GetHeight():0.f);
        x1 = this->pos.y + this->size.y;
        dd->pos.y = x0 + this->tabsx[n] * (x1 - x0) + (n==0?0.f:0.5f * barwidth);
        dd->size.y = (this->tabsx[n+1]-this->tabsx[n]) * (x1 - x0) - (n==0 || n==this->stack.size()-1?0.5f * barwidth:barwidth);
      } else {
        x0 = this->pos.x;
        x1 = this->pos.x + this->size.x;
        dd->pos.x = x0 + this->tabsx[n] * (x1 - x0) + (n==0?0.f:0.5f * barwidth);
        dd->size.x = (this->tabsx[n+1]-this->tabsx[n]) * (x1 - x0) - (n==0 || n==this->stack.size()-1?0.5f * barwidth:barwidth);
      }
      dd->parent = this;
      dd->drawRootContainer(root,lift,erased,ncount);
    }
  } else if (this->type == Dock::Type_Container) {
    // Draw the docked container window
    bool noresize = true;
    (*ncount)++;
    this->status = Dock::Status_Docked;
    this->hoverable = true;
    this->collapsed = root->collapsed;
    this->flags = ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|
      ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|
      ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoResize;
    if (this->currenttab){
      this->currenttab->hidden = root->collapsed;
      noresize = root->collapsed || !(*ncount == this->root->nchild);
      if (noresize)
        this->currenttab->flags |= ImGuiWindowFlags_NoResize;
      else
        this->currenttab->flags &= ~ImGuiWindowFlags_NoResize;
    }

    // only if the root is not collapsed
    if (!root->collapsed){
      bool transparentframe = this->currenttab || (this->dockflags & Dock::DockFlags_Transparent);
      this->hidden = false;
      if (this->currenttab)
        this->currenttab->hidden = false;

      // draw the window
      SetNextWindowPos(this->pos);
      SetNextWindowSize(this->size);
      SetNextWindowCollapsed(this->collapsed);
      if (this->currenttab)
        SetNextWindowContentSize(this->currenttab->window->SizeContents + ImVec2(0.f,this->tabdz));
      if (transparentframe)
        PushStyleColor(ImGuiCol_WindowBg,TransparentColor(ImGuiCol_WindowBg));
      Begin(this->label,nullptr,this->flags);

      // resize grip controlling the rootcontainer, if this is the
      // bottom-right window; lift grip if it is not.
      this->window = GetCurrentWindow();
      if (!this->currenttab && this->window){
        if (*ncount == this->root->nchild && !(this->root->flags & ImGuiWindowFlags_NoResize)){
          bool dclicked;
          ResizeGripOther(this->label, this->window, this->root->window);
          if (dclicked)
            this->root->resetRootContainerBars();
        }
        if (!this->automatic)
          if (!(this->dockflags & Dock::DockFlags_NoLiftContainer) && LiftGrip(this->label, this->window))
            *lift = this;
      }

      // write down the rest of the variables and end the window
      dockwin[this->window] = this;
      this->drawContainer(noresize,erased);
      this->tabdz = this->tabbarrect.Max.y - this->pos.y;
      End();
      if (transparentframe)
        PopStyleColor();

      // focus if clicked
      if (g->IO.MouseClicked[0] && g->HoveredRootWindow == this->window)
        this->focusContainer();

      // rootcontainer -> container -> dock
      placeWindow(this->root->window,this->window,+1);
      if (this->currenttab)
        placeWindow(this->window,this->currenttab->window,+1);

    } // !(root->collapsed)
  } // this->type == xx
}

void Dock::setDetachedDockPosition(float x, float y){
  this->pos_saved.x = x;
  this->pos_saved.y = y;
}

void Dock::setDetachedDockSize(float x, float y){
  this->size_saved.x = x;
  this->size_saved.y = y;
}

void Dock::setSplitWeight(float wx, float wy){
  this->splitweight = {wx,wy};
}

void Dock::closeDock() {
  if (this->status == Dock::Status_Docked){
    Dock *dpar = this->parent;
    this->unDock();
    if (dpar){
      Dock *ddlast = nullptr;
      for (auto dd : dpar->stack) {
	if (dd == this)
	  break;
	ddlast = dd;
      }
      dpar->stack.remove(this);
      if (dpar->currenttab == this){
      	if (dpar->stack.size())
	  if (ddlast)
	    dpar->currenttab = ddlast;
	  else
	    dpar->currenttab = dpar->stack.front();
      	else
      	  dpar->currenttab = nullptr;
      }
      dpar->killContainerMaybe();
    }
    this->status = Dock::Status_Closed;
  }
}

//xx// Public interface //xx//

Dock *ImGui::RootContainer(const char* label, bool* p_open /*=nullptr*/, ImGuiWindowFlags extra_flags /*= 0*/,
                       DockFlags dock_flags/*=0*/){
  bool collapsed;
  ImGuiContext *g = GetCurrentContext();
  ImGuiWindowFlags flags = extra_flags;

  Dock *dd = dockht[string(label)];
  if (!dd){
    dd = new Dock;
    IM_ASSERT(dd);
    dd->label = ImStrdup(label);
    dockht[string(dd->label)] = dd;
    dd->type = Dock::Type_Root;
  }
  dd->dockflags = dock_flags;

  // Initialize with a container if empty
  dd->OpRoot_FillEmpty();

  // Set the minimum size
  ImVec2 minsize, autosize;
  dd->getMinSize(&minsize, &autosize);
  SetNextWindowSizeConstraints(minsize,ImVec2(FLT_MAX,FLT_MAX),nullptr,nullptr);
  SetNextWindowContentSize(autosize);

  // Making an invisible window (always has a container)
  PushStyleColor(ImGuiCol_WindowBg,TransparentColor(ImGuiCol_WindowBg));
  flags = flags | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
  collapsed = !Begin(label,p_open,flags);

  // set the properties of the rootcontainer window
  dd->window = GetCurrentWindow();
  dd->pos = dd->window->Pos + ImVec2(0.f,dd->window->TitleBarHeight());
  dd->size = dd->window->Size - ImVec2(0.f,dd->window->TitleBarHeight());
  dd->type = Dock::Type_Root;
  dd->flags = extra_flags;
  dd->root = dd;
  dd->collapsed = collapsed;
  dockwin[dd->window] = dd;
  dd->p_open = p_open;
  dd->hoverable = false;

  // Update the status
  if (g->ActiveId == GetCurrentWindow()->MoveId && g->IO.MouseDown[0]){
    // Dragging
    dd->status = Dock::Status_Dragged;
  } else {
    // Stationary -> open, closed, or collapsed
    if (!p_open || *p_open){
      if (collapsed){
        dd->status = Dock::Status_Collapsed;
      }
      else{
        dd->status = Dock::Status_Open;
      }
    } else {
      dd->status = Dock::Status_Closed;
    }
  }

  // If the root container has just been closed, detach all docked windows
  if (dd->status == Dock::Status_Closed)
    dd->clearRootContainer();

  // Traverse the tree and draw all the bars
  if (!collapsed && dd->status != Dock::Status_Closed)
    dd->drawRootContainerBars(dd);

  // End the root container window
  End();
  PopStyleColor();

  // Traverse the tree and draw all the containers
  Dock *lift = nullptr, *erased = nullptr;
  if (dd->status != Dock::Status_Closed)
    dd->drawRootContainer(dd,&lift,&erased);

  // Clean up automatic containers
  if (erased)
    erased->killContainerMaybe();

  // Lift any container?
  if (dd->status != Dock::Status_Closed && lift)
    lift->liftContainer();

  return dd;
}

Dock *ImGui::Container(const char* label, bool* p_open /*=nullptr*/, ImGuiWindowFlags extra_flags /*= 0*/,
                       DockFlags dock_flags/*=0*/){

  bool collapsed = true;
  ImGuiContext *g = GetCurrentContext();
  ImGuiWindowFlags flags = extra_flags;

  Dock *dd = dockht[string(label)];
  if (!dd){
    dd = new Dock;
    IM_ASSERT(dd);
    dd->label = ImStrdup(label);
    dockht[string(dd->label)] = dd;
    dd->type = Dock::Type_Container;
  }
  dd->dockflags = dock_flags;

  // If docked, the root container takes care of everything. Clear next window data.
  if (dd->status == Dock::Status_Docked){
    g->SetNextWindowPosCond = g->SetNextWindowSizeCond = g->SetNextWindowContentSizeCond = g->SetNextWindowCollapsedCond = 0;
    g->SetNextWindowSizeConstraint = g->SetNextWindowFocus = false;
    return dd;
  }

  // Set the position, size, etc. if it was controlled by a root container
  if (dd->control_window_this_frame){
    dd->control_window_this_frame = false;
    SetNextWindowPos(dd->pos);
    SetNextWindowSize(dd->size);
    SetNextWindowCollapsed(dd->collapsed);
  }

  // Set the minimum and the contents size.
  ImVec2 minsize, autosize;
  dd->getMinSize(&minsize,&autosize);
  SetNextWindowSizeConstraints(minsize,ImVec2(FLT_MAX,FLT_MAX),nullptr,nullptr);
  SetNextWindowContentSize(autosize);
  if (dd->currenttab)
    flags = flags | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;

  // Render any container widgets in here
  bool transparentframe = dd->currenttab || (dd->dockflags & Dock::DockFlags_Transparent);
  if (transparentframe)
    PushStyleColor(ImGuiCol_WindowBg,TransparentColor(ImGuiCol_WindowBg));
  collapsed = !Begin(label,p_open,flags);

  // Fill the info for this dock
  dd->pos = GetWindowPos();
  dd->size = GetWindowSize();
  dd->type = Dock::Type_Container;
  dd->flags = extra_flags;
  dd->flags_saved = dd->flags;
  dd->root = nullptr;
  dd->collapsed = collapsed;
  dd->pos_saved = dd->pos;
  if (!collapsed) dd->size_saved = dd->size;
  dd->collapsed_saved = dd->collapsed;
  dd->window = GetCurrentWindow();
  dockwin[dd->window] = dd;
  dd->p_open = p_open;
  dd->parent = nullptr;
  dd->root = nullptr;

  // Update the status
  Dock *ddest;
  if (g->ActiveId == GetCurrentWindow()->MoveId && g->IO.MouseDown[0]){
    // Dragging
    dd->status = Dock::Status_Dragged;
    dd->hoverable = false;
    ddest = FindHoveredDock(Dock::Type_Container);
  } else {
    int ithis = -1;
    Dock::Drop_ iedge = Dock::Drop_None;
    bool dropit = dd->showingdrops && (dd->status == Dock::Status_Dragged && (ddest = FindHoveredDock(Dock::Type_Container)));
    if (dropit && ddest->stack.empty() && ddest->automatic && ddest->parent && 
	ddest->IsMouseHoveringFull() && ddest->parent->type == Dock::Type_Root){
      // drop it into the root container and replace it
      ddest->newDockRoot(dd,Dock::Drop_Tab);
    } else if (dropit && ddest->status == Dock::Status_Docked && 
	       !(ddest->automatic && ddest->stack.empty()) &&
	       ((iedge = ddest->IsMouseHoveringEdge()) > 0)){
      // drop into the edge
      ddest->newDockRoot(dd,iedge);
    } else {
      // Stationary -> open, closed, or collapsed
      if (!p_open || *p_open){
        if (collapsed){
          dd->status = Dock::Status_Collapsed;
          dd->hoverable = true;
        }
        else{
          dd->status = Dock::Status_Open;
          dd->hoverable = true;
        }
      } else {
        dd->status = Dock::Status_Closed;
        dd->hoverable = false;
      }
    }
  }

  // If dragged and hovering over a container, show the drop rectangles
  dd->showingdrops = dd->status == Dock::Status_Dragged && IsMouseDragging();
  if (dd->showingdrops&& ddest){
    if (ddest->parent && (ddest->parent->type == Dock::Type_Root || 
			  ddest->parent->type == Dock::Type_Horizontal || 
			  ddest->parent->type == Dock::Type_Vertical)){
      if (ddest->stack.empty() && ddest->automatic)
        ddest->showDropTargetFull();
      else
	ddest->showDropTargetEdge(ddest->IsMouseHoveringEdge(),true);
    }
  }

  // If the container has just been closed, detach all docked windows
  if (dd->status == Dock::Status_Closed && !dd->stack.empty())
    dd->clearContainer();

  // Draw the container elements
  Dock *erased = nullptr;
  dd->drawContainer(extra_flags & ImGuiWindowFlags_NoResize,&erased);
  dd->tabdz = dd->tabbarrect.Max.y - dd->pos.y;

  // If the container is clicked, set the correct hovered/moved flags
  // and raise container & docked window to the top of the stack.
  if (dd->currenttab && g->IO.MouseClicked[0] && g->HoveredRootWindow == dd->window)
    dd->focusContainer();

  // Put the current tab on top of the current window
  if (dd->currenttab)
    placeWindow(dd->window,dd->currenttab->window,+1);

  End();
  if (transparentframe)
    PopStyleColor();

  // Try to kill the container if a tab has just been erased
  if (erased)
    dd->killContainerMaybe();

  return dd;
}

bool ImGui::BeginDock(const char* label, bool* p_open /*=nullptr*/, ImGuiWindowFlags flags /*= 0*/, 
                       DockFlags dock_flags/*=0*/, Dock* oncedock /*=nullptr*/){
  bool collapsed;
  ImGuiContext *g = GetCurrentContext();

  // Create the entry in the dock context if it doesn't exist
  Dock *dd = dockht[string(label)];
  if (!dd) {
    dd = new Dock;
    IM_ASSERT(dd);
    dd->label = ImStrdup(label);
    dockht[string(dd->label)] = dd;
    dd->type = Dock::Type_Dock;
    dd->root = nullptr;

    // This is the first pass -> if oncedock exists, dock to that container
    if (oncedock){
      oncedock->newDock(dd,-1);
      dd->control_window_this_frame = true;
      dd->showTabWindow(oncedock,dd->flags & ImGuiWindowFlags_NoResize);
      dd->parent = oncedock;
    }
  }
  currentdock = dd;
  dd->dockflags = dock_flags;

  dd->noborder = false;
  if (dd->dockflags & Dock::DockFlags_Transparent)
    PushStyleColor(ImGuiCol_WindowBg,TransparentColor(ImGuiCol_WindowBg));
  if (dd->status == Dock::Status_Docked || dd->control_window_this_frame){
    // Docked or lifted: position and size are controlled
    dd->control_window_this_frame = false;
    SetNextWindowPos(dd->pos);
    SetNextWindowSize(dd->size);
    SetNextWindowCollapsed(dd->collapsed);
    if (dd->status == Dock::Status_Docked) {
      // Docked: flags and hidden controlled by the container, too
      flags = dd->flags | ImGuiWindowFlags_NoResize;
      collapsed = dd->hidden;
      if (dd->hidden){
	dd->noborder = true;
	PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        Begin(label,nullptr,dd->size,0.0,flags);
      } else {
        Begin(label,nullptr,flags);
      }
      dd->root = dd->parent->root;

      // lift and resize grips
      if (dd->window && !dd->hidden && !dd->collapsed){
        if (dd->root && dd->root->window){
          // For the root window. This dock can be resized if the root
          // can be resized and the dd->flags is not
          // noresize. dd->flags is set by the docked container based
          // on the nchild.
          if (!(dd->root->flags & ImGuiWindowFlags_NoResize) && !(dd->flags & ImGuiWindowFlags_NoResize)){
            bool dclicked;
            ResizeGripOther(dd->label, dd->window, dd->root->window, &dclicked);
            if (dclicked)
              dd->root->resetRootContainerBars();
          }
          if (!dd->parent->automatic && !(dd->parent->dockflags & Dock::DockFlags_NoLiftContainer) && 
              LiftGrip(dd->label, dd->window))
              dd->parent->liftContainer();
        } else if (dd->parent && dd->parent->window && !(dd->parent->flags & ImGuiWindowFlags_NoResize)){
          // for the parent container
          ResizeGripOther(dd->label, dd->window, dd->parent->window);
        }
      }
    } else if (dd->status == Dock::Status_Dragged) {
      // the window has just been lifted from a container. Go back to
      // being a normal window with the new position and size; being
      // dragged.
      collapsed = !Begin(label,p_open,flags);
      dd->window = GetCurrentWindow();
      dockwin[dd->window] = dd;
      g->MovingWindow = dd->window;
      g->MovingWindowMoveId = dd->window->RootWindow->MoveId;
      SetActiveID(g->MovingWindowMoveId, dd->window->RootWindow);
      dd->parent = nullptr;
      dd->root = nullptr;
    } else {
      // the window has just been lifted, but not dragging
      collapsed = !Begin(label,p_open,flags);
      dd->window = GetCurrentWindow();
      dockwin[dd->window] = dd;
      dd->parent = nullptr;
      dd->root = nullptr;
    }
  } else {
    // Floating window
    collapsed = !Begin(label,p_open,flags);
    dd->collapsed_saved = collapsed;
    dd->pos_saved = dd->pos;
    if (!collapsed) dd->size_saved = dd->size;
    dd->flags_saved = flags;
    dd->root = nullptr;
  }

  // Fill the info for this dock
  dd->pos = GetWindowPos();
  dd->size = GetWindowSize();
  dd->type = Dock::Type_Dock;
  dd->flags = flags;
  dd->collapsed = collapsed;
  dd->window = GetCurrentWindow();
  dockwin[dd->window] = dd;
  dd->p_open = p_open;
  dd->control_window_this_frame = false;

  // Update the status
  Dock *ddest = nullptr;

  // is the window being dragged directly?
  bool isdragged = g->ActiveId == GetCurrentWindow()->MoveId && g->IO.MouseDown[0];

  // ... or maybe it is grabbed from a child window?
  if (!isdragged && g->IO.MouseDown[0] && g->MovingWindow && 
      g->MovingWindow->Flags & ImGuiWindowFlags_ChildWindow && g->MovingWindow->RootWindow)
    isdragged = (g->MovingWindow->RootWindow == GetCurrentWindow());

  if (isdragged){
    // Dragging
    dd->status = Dock::Status_Dragged;
    dd->hoverable = false;
    ddest = FindHoveredDock(Dock::Type_Container);
  } else {
    int ithis = -1;
    Dock::Drop_ iedge = Dock::Drop_None;
    bool dropit = dd->showingdrops && dd->status == Dock::Status_Dragged && (ddest = FindHoveredDock(Dock::Type_Container));
    if (dropit && (ddest->stack.empty() && ddest->IsMouseHoveringFull() || ((ithis = ddest->getNearestTabBorder()) >= 0))){
      // Just stopped dragging and there is a container below
      ddest->newDock(dd,ithis);
    } else if (dropit && ddest->status == Dock::Status_Docked && !(ddest->automatic && ddest->stack.empty()) && 
	       ((iedge = ddest->IsMouseHoveringEdge()) > 0)){
      // stopped dragging and there is a root container below
      ddest->newDockRoot(dd,iedge);
    } else if (dd->status != Dock::Status_Docked){
      // Stationary -> open, closed, or collapsed
      if (!p_open || *p_open){
        if (collapsed){
          dd->status = Dock::Status_Collapsed;
          dd->hoverable = true;
        }
        else{
          dd->status = Dock::Status_Open;
          dd->hoverable = true;
        }
      } else {
        dd->status= Dock::Status_Closed;
        dd->hoverable = false;
      }
    }
  }

  // If dragged and hovering over a container, show the drop rectangles
  dd->showingdrops = dd->status == Dock::Status_Dragged && IsMouseDragging();
  if (dd->showingdrops && ddest){
    if (ddest->stack.empty()){
      ddest->showDropTargetFull();
      if (ddest->status == Dock::Status_Docked && !(ddest->automatic))
	ddest->showDropTargetEdge(ddest->IsMouseHoveringEdge(),true);
    } else if (ddest->IsMouseHoveringTabBar()) {
      ddest->showDropTargetOnTabBar();
    } else if (ddest->status == Dock::Status_Docked) {
      ddest->showDropTargetEdge(ddest->IsMouseHoveringEdge(),true);
    }
  }

  // If this window is being clicked or dragged, focus the container+dock
  if (dd->status == Dock::Status_Docked && !dd->hidden)
    if (g->HoveredWindow == dd->window && g->IO.MouseClicked[0])
      dd->parent->focusContainer();

  if (dd->status == Dock::Status_Docked && !dd->hidden){
    // Move the container right under this dock or, if either is
    // noraise, move the dock on top of the container
    if (dd->parent->flags & ImGuiWindowFlags_NoBringToFrontOnFocus ||
        dd->flags & ImGuiWindowFlags_NoBringToFrontOnFocus)
      placeWindow(dd->parent->window,dd->window,+1);
    else
      placeWindow(dd->window,dd->parent->window,-1);
  }

  return !collapsed;
}

Dock *ImGui::GetCurrentDock() {
  return currentdock;
}

void ImGui::EndDock() {
  End();
  if (currentdock->dockflags & Dock::DockFlags_Transparent)
    PopStyleColor();
  if (currentdock->noborder)
    PopStyleVar();
  currentdock = nullptr;
}

void ImGui::PrintDock__() {
  ImGuiContext *g = GetCurrentContext();
  // for (auto dock : dockht){
  //   Text("key=%s id=%d label=%s\n", dock.first,dock.second->label);
  //   Text("pos=(%f,%f) size=(%f,%f)\n",dock.second->pos.x,dock.second->pos.y,dock.second->size.x,dock.second->size.y);
  //   Text("type=%d status=%d\n", dock.second->type, dock.second->status);
  //   Text("sttype=%d list_size=%d\n", dock.second->sttype, dock.second->stack.size());
  //   if (dock.second->p_open)
  //     Text("p_open=%d\n", *(dock.second->p_open));
  // }

  // Text("activeid: %d\n",g->ActiveId);
  // Text("activeidwindow: %p movingwindow: %p\n",g->ActiveIdWindow,g->MovingWindow);
  // if (g->ActiveIdWindow){
  //   Text("ischild: %d\n",g->ActiveIdWindow->Flags & ImGuiWindowFlags_ChildWindow);
  //   Text("rootwindow: %p\n",g->ActiveIdWindow->RootWindow);
  // }
  // if (g->MovingWindow){
  //   Text("ischild: %d\n",g->MovingWindow->Flags & ImGuiWindowFlags_ChildWindow);
  //   Text("rootwindow: %p\n",g->MovingWindow->RootWindow);
  // }
  // Separator();
  for (auto dock : dockht){
    Text("label=%s id=%p type=%d status=%d\n",dock.second->label,
	 dock.second,dock.second->type,dock.second->status);
    // if (dock.second->window)
    //   Text("moveid=%d\n",dock.second->window->MoveId);
    Separator();
  }

  // if (g->HoveredWindow)
  //   Text("Hovered: %s\n",g->HoveredWindow->Name);
  // else
  //   Text("Hovered: none\n");
  // if (g->HoveredRootWindow)
  //   Text("HoveredRoot: %s\n",g->HoveredRootWindow->Name);
  // else
  //   Text("HoveredRoot: none\n");
  // if (g->IO.MouseClicked[0])
  //   Text("Mouse clicked!\n");
  // else
  //   Text("Mouse not clicked!\n");
  // for (int i = 0; i < g->Windows.Size; i++){
  //   Text("%d %s %f %f\n",i,g->Windows[i]->Name,g->Windows[i]->Pos.x,g->Windows[i]->Pos.y);
  // }
}

void ImGui::ShutdownDock(){
  for (auto dpair : dockht){
    if (dpair.second) delete dpair.second;
  }
  dockht.clear();
  dockwin.clear();
}

