#include "Settings.h"
#include "Event.h"

#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

#include <cstdio>
#include <cstdlib>


static const char* kSettingsFile = "TeslaViewer";


Settings::Settings()
	:
	path("/boot/home/Desktop/Tesla"),
	frame(40, 40, 1200, 740),
	speed(1.0f),
	frameValid(false)
{
}


static BPath
SettingsPath()
{
	BPath p;
	find_directory(B_USER_SETTINGS_DIRECTORY, &p);
	p.Append(kSettingsFile);
	return p;
}


void
Settings::Load()
{
	BPath p = SettingsPath();
	BFile file(p.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK) return;

	BMessage msg;
	if (msg.Unflatten(&file) != B_OK) return;

	BString tmp;
	if (msg.FindString("path", &tmp) == B_OK && tmp.Length() > 0)
		path = tmp;

	BRect r;
	if (msg.FindRect("frame", &r) == B_OK
		&& r.Width() > 100 && r.Height() > 100) {
		frame = r;
		frameValid = true;
	}

	float s = 0;
	if (msg.FindFloat("speed", &s) == B_OK && s > 0)
		speed = s;

	cameraMap.clear();
	BMessage cmap;
	if (msg.FindMessage("cameraMap", &cmap) == B_OK) {
		char* name = NULL;
		type_code type;
		int32 count;
		for (int32 i = 0;
				cmap.GetInfo(B_INT32_TYPE, i, &name, &type, &count) == B_OK;
				i++) {
			int32 grid = -1;
			if (cmap.FindInt32(name, &grid) == B_OK)
				cameraMap[atoi(name)] = grid;
		}
	}
}


void
Settings::Save()
{
	BMessage msg;
	msg.AddString("path", path);
	msg.AddRect("frame", frame);
	msg.AddFloat("speed", speed);

	BMessage cmap;
	for (const auto& kv : cameraMap) {
		char key[16];
		snprintf(key, sizeof(key), "%d", (int)kv.first);
		cmap.AddInt32(key, kv.second);
	}
	msg.AddMessage("cameraMap", &cmap);

	BPath p = SettingsPath();
	BFile file(p.Path(),
		B_CREATE_FILE | B_WRITE_ONLY | B_ERASE_FILE);
	if (file.InitCheck() == B_OK)
		msg.Flatten(&file);
}


int32
Settings::CameraToGrid(int32 cameraId) const
{
	auto it = cameraMap.find(cameraId);
	if (it != cameraMap.end()) return it->second;
	return TriggerCameraToGrid(cameraId);
}


void
Settings::SetCameraMapping(int32 cameraId, int32 gridSlot)
{
	if (gridSlot < 0)
		cameraMap.erase(cameraId);
	else
		cameraMap[cameraId] = gridSlot;
}
