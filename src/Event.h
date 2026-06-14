#ifndef TESLA_EVENT_H
#define TESLA_EVENT_H

#include <String.h>
#include <List.h>
#include <vector>

enum CameraId {
	CAM_FRONT = 0,
	CAM_BACK = 1,
	CAM_LEFT = 2,
	CAM_RIGHT = 3,
	CAM_COUNT = 4
};

const char* CameraName(CameraId id);

// Maps the numeric "camera" field from Tesla's event.json to one of our
// 4 grid positions. Returns -1 if unknown.
int TriggerCameraToGrid(int32 cam);

// Returns a friendly name for the raw numeric value in event.json.
const char* TriggerCameraLabel(int32 cam);

struct Clip {
	BString time;
	BString path[CAM_COUNT];
};

struct Event {
	BString		folderPath;
	BString		folderName;

	BString		timestamp;
	BString		city;
	BString		street;
	BString		estLat;
	BString		estLon;
	BString		reason;
	int32		camera;

	std::vector<Clip> clips;

	BString DisplayLabel() const;

	// Global timeline (in microseconds since the start of the first clip)
	bigtime_t	ClipStartMicros(size_t i) const;
	bigtime_t	AlarmGlobalMicros() const;	// negative if unknown
	bigtime_t	ClipNominalDurationMicros(size_t i) const;
	// Find the clip index containing global time t. Sets localOffset to the
	// position inside that clip. Returns -1 if t is past the last clip.
	int32		FindClipForGlobalTime(bigtime_t t, bigtime_t& localOffset) const;
};

// Parse "YYYY-MM-DD_HH-MM-SS" (clip filename) or "YYYY-MM-DDTHH:MM:SS"
// (event.json) into seconds since epoch (UTC-agnostic — we only care
// about deltas). Returns -1 on error.
int64 ParseTeslaTimestamp(const char* s);

class EventScanner {
public:
	static bool Scan(const char* rootDir, std::vector<Event>& out);

private:
	static bool LoadEventJson(const char* path, Event& ev);
	static void CollectClips(const char* dir, Event& ev);
};

#endif
