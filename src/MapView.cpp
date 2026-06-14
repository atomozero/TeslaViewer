#include "MapView.h"

#include "Messages.h"

#include <Bitmap.h>
#include <Cursor.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Region.h>
#include <TranslationUtils.h>
#include <Window.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>


static const int kTileSize = 256;
static const int kMaxZoom = 18;
static const int kMinZoom = 2;
static const int kPinRadius = 6;
static const uint32 kMsgTileReady = 'tilR';


static int64
TileKey(int z, int x, int y)
{
	return ((int64)z << 48) | ((int64)x << 24) | (int64)y;
}


struct TileRequest {
	int			z;
	int			x;
	int			y;
	BMessenger	reply;
};


static int32
TileFetcherThread(void* data)
{
	TileRequest* req = (TileRequest*)data;

	BPath cacheRoot;
	find_directory(B_USER_CACHE_DIRECTORY, &cacheRoot);
	cacheRoot.Append("TeslaViewer");
	cacheRoot.Append("tiles");
	BString zDir;
	zDir.SetToFormat("%d", req->z);
	cacheRoot.Append(zDir);
	BString xDir;
	xDir.SetToFormat("%d", req->x);
	cacheRoot.Append(xDir);
	create_directory(cacheRoot.Path(), 0755);

	BString tilePath = cacheRoot.Path();
	tilePath << "/" << req->y << ".png";
	BString tmpPath = tilePath;
	tmpPath << ".tmp";

	// Download to .tmp and rename atomically so the main thread never
	// sees a partial PNG on disk.
	BString cmd;
	cmd.SetToFormat(
		"curl -s --max-time 15 -A 'TeslaViewer/1.0 (Haiku)' "
		"-o '%s' 'https://tile.openstreetmap.org/%d/%d/%d.png'"
		" && mv '%s' '%s'",
		tmpPath.String(), req->z, req->x, req->y,
		tmpPath.String(), tilePath.String());

	int rc = system(cmd.String());

	BMessage m(kMsgTileReady);
	m.AddInt32("z", req->z);
	m.AddInt32("x", req->x);
	m.AddInt32("y", req->y);
	m.AddInt32("rc", rc);
	req->reply.SendMessage(&m);

	delete req;
	return 0;
}


MapView::MapView(const char* name, const std::vector<Event>* events,
		BMessenger eventSelectedTarget)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS),
	fEvents(events),
	fEventSelectedTarget(eventSelectedTarget),
	fZoom(13),
	fCenterLat(45.49),	// Mestre default
	fCenterLon(12.24),
	fDragging(false),
	fDragStartLat(0),
	fDragStartLon(0)
{
	SetViewColor(180, 200, 220);
	FitAllEvents();
}


MapView::~MapView()
{
	for (auto& kv : fTileCache) delete kv.second;
}


void
MapView::SetEvents(const std::vector<Event>* events)
{
	fEvents = events;
	FitAllEvents();
	Invalidate();
}


void
MapView::CenterOn(double lat, double lon, int32 zoom)
{
	fCenterLat = lat;
	fCenterLon = lon;
	if (zoom >= kMinZoom && zoom <= kMaxZoom) fZoom = zoom;
	Invalidate();
}


void
MapView::FitAllEvents()
{
	if (fEvents == NULL || fEvents->empty()) return;
	double minLat = 90, maxLat = -90, minLon = 180, maxLon = -180;
	int count = 0;
	for (const Event& e : *fEvents) {
		if (e.estLat.IsEmpty() || e.estLon.IsEmpty()) continue;
		double lat = atof(e.estLat.String());
		double lon = atof(e.estLon.String());
		if (lat < minLat) minLat = lat;
		if (lat > maxLat) maxLat = lat;
		if (lon < minLon) minLon = lon;
		if (lon > maxLon) maxLon = lon;
		count++;
	}
	if (count == 0) return;
	fCenterLat = (minLat + maxLat) / 2.0;
	fCenterLon = (minLon + maxLon) / 2.0;
	// pick a zoom: rough heuristic
	double span = std::max(maxLat - minLat, maxLon - minLon);
	if (span < 0.01) fZoom = 16;
	else if (span < 0.05) fZoom = 14;
	else if (span < 0.2) fZoom = 12;
	else if (span < 1) fZoom = 10;
	else if (span < 5) fZoom = 8;
	else fZoom = 6;
}


void
MapView::_LatLonToWorld(double lat, double lon, double& wx, double& wy) const
{
	double n = (double)((int64)1 << fZoom);
	wx = (lon + 180.0) / 360.0 * n * kTileSize;
	double sinL = sin(lat * M_PI / 180.0);
	wy = (0.5 - log((1.0 + sinL) / (1.0 - sinL)) / (4.0 * M_PI))
		* n * kTileSize;
}


void
MapView::_WorldToLatLon(double wx, double wy, double& lat, double& lon) const
{
	double n = (double)((int64)1 << fZoom);
	lon = wx / (n * kTileSize) * 360.0 - 180.0;
	double m = M_PI - 2.0 * M_PI * wy / (n * kTileSize);
	lat = 180.0 / M_PI * atan(0.5 * (exp(m) - exp(-m)));
}


BString
MapView::_TileDir(int z, int x) const
{
	BPath p;
	find_directory(B_USER_CACHE_DIRECTORY, &p);
	p.Append("TeslaViewer");
	p.Append("tiles");
	BString s;
	s.SetToFormat("%s/%d/%d", p.Path(), z, x);
	return s;
}


BString
MapView::_TilePath(int z, int x, int y) const
{
	BString s;
	s.SetToFormat("%s/%d.png", _TileDir(z, x).String(), y);
	return s;
}


BBitmap*
MapView::_GetCachedTile(int z, int x, int y)
{
	int64 key = TileKey(z, x, y);
	auto it = fTileCache.find(key);
	if (it != fTileCache.end()) return it->second;

	// Don't try to read a tile that's still being downloaded.
	if (fPendingTiles.find(key) != fPendingTiles.end()) return NULL;

	BString p = _TilePath(z, x, y);
	BEntry entry(p.String());
	if (entry.Exists()) {
		BBitmap* bmp = BTranslationUtils::GetBitmapFile(p.String());
		if (bmp != NULL && bmp->IsValid()) {
			fTileCache[key] = bmp;
			return bmp;
		}
		delete bmp;
	}
	return NULL;
}


void
MapView::_FetchTile(int z, int x, int y)
{
	int64 key = TileKey(z, x, y);
	if (fPendingTiles.find(key) != fPendingTiles.end()) return;
	fPendingTiles.insert(key);

	TileRequest* req = new TileRequest();
	req->z = z;
	req->x = x;
	req->y = y;
	req->reply = BMessenger(this);

	thread_id tid = spawn_thread(TileFetcherThread, "tile_fetch",
		B_LOW_PRIORITY, req);
	resume_thread(tid);
}


void
MapView::Draw(BRect updateRect)
{
	BRect b = Bounds();
	float w = b.Width(), h = b.Height();

	double cx, cy;
	_LatLonToWorld(fCenterLat, fCenterLon, cx, cy);

	double worldLeft = cx - w / 2.0;
	double worldTop  = cy - h / 2.0;

	int tileLeft = (int)floor(worldLeft / kTileSize);
	int tileTop  = (int)floor(worldTop / kTileSize);
	int tileRight = (int)floor((worldLeft + w) / kTileSize);
	int tileBottom = (int)floor((worldTop + h) / kTileSize);

	int worldTiles = 1 << fZoom;

	SetDrawingMode(B_OP_COPY);
	for (int tx = tileLeft; tx <= tileRight; tx++) {
		for (int ty = tileTop; ty <= tileBottom; ty++) {
			if (ty < 0 || ty >= worldTiles) continue;
			int wrappedX = ((tx % worldTiles) + worldTiles) % worldTiles;
			BBitmap* bmp = _GetCachedTile(fZoom, wrappedX, ty);
			float dx = tx * kTileSize - worldLeft;
			float dy = ty * kTileSize - worldTop;
			BRect dst(dx, dy, dx + kTileSize - 1, dy + kTileSize - 1);
			if (bmp != NULL) {
				DrawBitmap(bmp, dst);
			} else {
				SetHighColor(220, 220, 220);
				FillRect(dst);
				SetHighColor(180, 180, 180);
				StrokeRect(dst);
				_FetchTile(fZoom, wrappedX, ty);
			}
		}
	}

	// Pins for events with GPS
	SetDrawingMode(B_OP_ALPHA);
	if (fEvents != NULL) {
		for (size_t i = 0; i < fEvents->size(); i++) {
			const Event& e = (*fEvents)[i];
			if (e.estLat.IsEmpty() || e.estLon.IsEmpty()) continue;
			double lat = atof(e.estLat.String());
			double lon = atof(e.estLon.String());
			double pwx, pwy;
			_LatLonToWorld(lat, lon, pwx, pwy);
			float px = (float)(pwx - worldLeft);
			float py = (float)(pwy - worldTop);
			if (px < -kPinRadius || px > w + kPinRadius
				|| py < -kPinRadius * 2 || py > h + kPinRadius) continue;

			SetHighColor(0, 0, 0, 80);
			FillEllipse(BPoint(px, py + 2), kPinRadius + 1, 3);
			SetHighColor(220, 40, 40, 230);
			FillEllipse(BPoint(px, py), kPinRadius, kPinRadius);
			SetHighColor(255, 255, 255, 230);
			StrokeEllipse(BPoint(px, py), kPinRadius, kPinRadius);
		}
	}
	SetDrawingMode(B_OP_COPY);

	// Info corner
	SetHighColor(255, 255, 255, 200);
	SetDrawingMode(B_OP_ALPHA);
	FillRect(BRect(4, 4, 240, 22));
	SetDrawingMode(B_OP_OVER);
	SetHighColor(0, 0, 0);
	BString info;
	info.SetToFormat("z=%d  center %.4f, %.4f", (int)fZoom,
		fCenterLat, fCenterLon);
	DrawString(info.String(), BPoint(8, 18));
}


int32
MapView::_HitEvent(BPoint where) const
{
	if (fEvents == NULL) return -1;
	BRect b = Bounds();
	double cx, cy;
	_LatLonToWorld(fCenterLat, fCenterLon, cx, cy);
	double worldLeft = cx - b.Width() / 2.0;
	double worldTop  = cy - b.Height() / 2.0;

	float best = 1e9;
	int32 bestIdx = -1;
	for (size_t i = 0; i < fEvents->size(); i++) {
		const Event& e = (*fEvents)[i];
		if (e.estLat.IsEmpty() || e.estLon.IsEmpty()) continue;
		double lat = atof(e.estLat.String());
		double lon = atof(e.estLon.String());
		double pwx, pwy;
		_LatLonToWorld(lat, lon, pwx, pwy);
		float dx = (float)(pwx - worldLeft) - where.x;
		float dy = (float)(pwy - worldTop) - where.y;
		float d = dx * dx + dy * dy;
		if (d <= kPinRadius * kPinRadius * 4 && d < best) {
			best = d;
			bestIdx = (int32)i;
		}
	}
	return bestIdx;
}


void
MapView::MouseDown(BPoint where)
{
	int32 hit = _HitEvent(where);
	if (hit >= 0) {
		BMessage m(kMsgEventSelected);
		m.AddInt32("eventIndex", hit);
		fEventSelectedTarget.SendMessage(&m);
		return;
	}
	fDragging = true;
	fDragStart = where;
	fDragStartLat = fCenterLat;
	fDragStartLon = fCenterLon;
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
}


void
MapView::MouseUp(BPoint /*where*/)
{
	fDragging = false;
}


void
MapView::MouseMoved(BPoint where, uint32 /*code*/, const BMessage* /*msg*/)
{
	if (!fDragging) return;
	double cx, cy;
	_LatLonToWorld(fDragStartLat, fDragStartLon, cx, cy);
	cx -= (where.x - fDragStart.x);
	cy -= (where.y - fDragStart.y);
	_WorldToLatLon(cx, cy, fCenterLat, fCenterLon);
	Invalidate();
}


void
MapView::ScrollWheel(float /*deltaX*/, float deltaY)
{
	int newZoom = fZoom + (deltaY < 0 ? 1 : -1);
	if (newZoom < kMinZoom) newZoom = kMinZoom;
	if (newZoom > kMaxZoom) newZoom = kMaxZoom;
	if (newZoom == fZoom) return;
	fZoom = newZoom;
	Invalidate();
}


void
MapView::MessageReceived(BMessage* msg)
{
	if (msg->what == kMsgTileReady) {
		int32 z = 0, x = 0, y = 0, rc = -1;
		msg->FindInt32("z", &z);
		msg->FindInt32("x", &x);
		msg->FindInt32("y", &y);
		msg->FindInt32("rc", &rc);
		fPendingTiles.erase(TileKey(z, x, y));
		if (rc == 0) Invalidate();
		return;
	}
	BView::MessageReceived(msg);
}
