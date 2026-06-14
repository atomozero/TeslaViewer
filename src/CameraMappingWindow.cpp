#include "CameraMappingWindow.h"

#include "Event.h"
#include "Messages.h"
#include "Settings.h"

#include <Button.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Message.h>
#include <StringView.h>

#include <cstdio>


static const uint32 kMsgOK     = 'OK_M';
static const uint32 kMsgCancel = 'CncM';


CameraMappingWindow::CameraMappingWindow(BMessenger target,
		Settings* settings, const std::vector<int32>& detectedIds)
	:
	BWindow(BRect(200, 200, 540, 440), "Camera mapping",
		B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE
		| B_AUTO_UPDATE_SIZE_LIMITS),
	fSettings(settings),
	fTarget(target),
	fIds(detectedIds)
{
	BLayoutBuilder::Group<> root(this, B_VERTICAL, B_USE_DEFAULT_SPACING);
	root.SetInsets(B_USE_WINDOW_INSETS);

	BStringView* hint = new BStringView("hint",
		"Pick which physical camera each Tesla id maps to.");
	root.Add(hint);

	if (fIds.empty()) {
		root.Add(new BStringView("none",
			"No events loaded. Rescan first."));
	}

	for (size_t i = 0; i < fIds.size(); i++) {
		BMenu* menu = new BMenu("cam");
		menu->SetRadioMode(true);
		menu->SetLabelFromMarked(true);

		const char* names[] = {"Unknown",
			"Front", "Back", "Left repeater", "Right repeater"};
		int32 values[]  = {-1, CAM_FRONT, CAM_BACK, CAM_LEFT, CAM_RIGHT};
		int32 current = fSettings->CameraToGrid(fIds[i]);
		for (size_t k = 0; k < sizeof(values) / sizeof(values[0]); k++) {
			BMessage* m = new BMessage('iSel');
			m->AddInt32("grid", values[k]);
			BMenuItem* it = new BMenuItem(names[k], m);
			if (values[k] == current) it->SetMarked(true);
			menu->AddItem(it);
		}

		char label[32];
		snprintf(label, sizeof(label), "Camera id %d:", (int)fIds[i]);
		BMenuField* field = new BMenuField("cam", label, menu);
		fMenus.push_back(menu);
		root.Add(field);
	}

	root.AddGlue();
	root.AddGroup(B_HORIZONTAL)
		.AddGlue()
		.Add(new BButton("cancel", "Cancel", new BMessage(kMsgCancel)))
		.Add(new BButton("ok", "Apply", new BMessage(kMsgOK)))
	.End();
}


void
CameraMappingWindow::_Commit()
{
	for (size_t i = 0; i < fIds.size() && i < fMenus.size(); i++) {
		BMenuItem* marked = fMenus[i]->FindMarked();
		if (marked == NULL) continue;
		BMessage* m = marked->Message();
		if (m == NULL) continue;
		int32 grid = -1;
		m->FindInt32("grid", &grid);
		fSettings->SetCameraMapping(fIds[i], grid);
	}
	fSettings->Save();
	fTarget.SendMessage(kMsgCameraMapChanged);
}


void
CameraMappingWindow::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case kMsgOK:
			_Commit();
			Quit();
			break;
		case kMsgCancel:
			Quit();
			break;
		default:
			BWindow::MessageReceived(msg);
	}
}
