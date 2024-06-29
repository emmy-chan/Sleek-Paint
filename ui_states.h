#pragma once
#include "app.h"
#include "imgui.h"
#include "imfilebrowser.h"

class cUIStateNewProject : public cState {
public:
	void Update() override;
};

class cUIStateSaveProject : public cState {
public:
	void Update() override;
};

class cUIStateOpenProject : public cState {
public:
	void Update() override;
};

class cUIStateCanvasSize : public cState {
public:
	void Update() override;
};

class cUIStateSaveWarning : public cState {
public:
	void Update() override;
};

class cUIStateLoadPalette : public cState {
public:
	void Update() override;
};

class cUIStateSavePalette : public cState {
public:
	void Update() override;
};