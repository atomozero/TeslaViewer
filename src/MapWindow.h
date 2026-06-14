#ifndef TESLA_MAP_WINDOW_H
#define TESLA_MAP_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include <vector>

#include "Event.h"

class MapView;


class MapWindow : public BWindow {
public:
							MapWindow(const std::vector<Event>* events,
								BMessenger eventSelectedTarget);

	virtual void			MessageReceived(BMessage* msg);
	virtual bool			QuitRequested();

			void			RefreshEvents(const std::vector<Event>* events);
			void			CenterOn(double lat, double lon, int32 zoom = -1);

			void			SetCloseNotificationTarget(BMessenger target);

private:
			MapView*		fMap;
			BMessenger		fCloseNotify;
			bool			fHasCloseNotify;
};

#endif
