#include "MapWindow.h"

#include "MapView.h"
#include "Messages.h"

#include <Button.h>
#include <LayoutBuilder.h>


MapWindow::MapWindow(const std::vector<Event>* events,
		BMessenger eventSelectedTarget)
	:
	BWindow(BRect(100, 100, 900, 700), "Event map",
		B_TITLED_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fHasCloseNotify(false)
{
	fMap = new MapView("map", events, eventSelectedTarget);

	BButton* fitBtn = new BButton("fit", "Fit all",
		new BMessage('fitA'));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMap)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_SMALL_INSETS)
			.AddGlue()
			.Add(fitBtn)
		.End();
}


void
MapWindow::RefreshEvents(const std::vector<Event>* events)
{
	if (LockLooperWithTimeout(1000000) != B_OK) return;
	if (fMap != NULL) fMap->SetEvents(events);
	UnlockLooper();
}


void
MapWindow::CenterOn(double lat, double lon, int32 zoom)
{
	if (LockLooperWithTimeout(1000000) != B_OK) return;
	if (fMap != NULL) fMap->CenterOn(lat, lon, zoom);
	UnlockLooper();
}


void
MapWindow::SetCloseNotificationTarget(BMessenger target)
{
	fCloseNotify = target;
	fHasCloseNotify = true;
}


bool
MapWindow::QuitRequested()
{
	if (fHasCloseNotify)
		fCloseNotify.SendMessage(kMsgMapClosed);
	return true;
}


void
MapWindow::MessageReceived(BMessage* msg)
{
	if (msg->what == 'fitA') {
		if (fMap != NULL) {
			fMap->FitAllEvents();
			fMap->Invalidate();
		}
		return;
	}
	BWindow::MessageReceived(msg);
}
