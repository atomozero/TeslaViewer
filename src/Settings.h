#ifndef TESLA_SETTINGS_H
#define TESLA_SETTINGS_H

#include <Rect.h>
#include <String.h>

#include <map>


class Settings {
public:
							Settings();

			void			Load();
			void			Save();

	// Returns the grid slot (0..3) for the given event.json camera id.
	// Consults the user override map first, then falls back to the
	// default best-guess mapping in Event.cpp. Returns -1 if unknown.
			int32			CameraToGrid(int32 cameraId) const;
			void			SetCameraMapping(int32 cameraId, int32 gridSlot);

			BString			path;
			BRect			frame;
			float			speed;
			bool			frameValid;
			std::map<int32, int32>	cameraMap;	// id -> grid (-1 = unset)
};

#endif
