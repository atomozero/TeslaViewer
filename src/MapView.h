#ifndef TESLA_MAP_VIEW_H
#define TESLA_MAP_VIEW_H

#include <Messenger.h>
#include <Point.h>
#include <String.h>
#include <View.h>

#include <map>
#include <set>
#include <vector>

#include "Event.h"

class BBitmap;


// Slippy-map view that fetches OpenStreetMap tiles into a disk cache
// and draws clickable pins for each event with GPS data.
class MapView : public BView {
public:
							MapView(const char* name,
								const std::vector<Event>* events,
								BMessenger eventSelectedTarget);
	virtual					~MapView();

	virtual void			Draw(BRect updateRect);
	virtual void			MouseDown(BPoint where);
	virtual void			MouseUp(BPoint where);
	virtual void			MouseMoved(BPoint where, uint32 code,
								const BMessage* msg);
	virtual void			ScrollWheel(float deltaX, float deltaY);
	virtual void			MessageReceived(BMessage* msg);

			void			SetEvents(const std::vector<Event>* events);
			void			FitAllEvents();
			void			CenterOn(double lat, double lon, int32 zoom = -1);

private:
			void			_FetchTile(int z, int x, int y);
			BBitmap*		_GetCachedTile(int z, int x, int y);
			BString			_TilePath(int z, int x, int y) const;
			BString			_TileDir(int z, int x) const;

	// Coordinate conversions
			void			_LatLonToWorld(double lat, double lon,
								double& wx, double& wy) const;
			void			_WorldToLatLon(double wx, double wy,
								double& lat, double& lon) const;

			int32			_HitEvent(BPoint where) const;

private:
			const std::vector<Event>*	fEvents;
			BMessenger			fEventSelectedTarget;

			int32				fZoom;		// 1..18
			double				fCenterLat;
			double				fCenterLon;

			bool				fDragging;
			BPoint				fDragStart;
			double				fDragStartLat;
			double				fDragStartLon;

			std::map<int64, BBitmap*>	fTileCache; // key = z<<48 | x<<24 | y
			std::set<int64>				fPendingTiles;
};

#endif
