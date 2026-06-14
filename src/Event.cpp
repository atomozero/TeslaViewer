#include "Event.h"

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <Path.h>
#include <StorageDefs.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <map>
#include <string>


const char*
CameraName(CameraId id)
{
	switch (id) {
		case CAM_FRONT: return "Front";
		case CAM_BACK:  return "Back";
		case CAM_LEFT:  return "Left repeater";
		case CAM_RIGHT: return "Right repeater";
		default:        return "?";
	}
}


// Tesla's event.json uses numeric camera ids that do not match a fixed
// public mapping; observed values: 0 (front), 6 (back), 5 (a side
// camera). Unknown ids return -1 and the trigger highlight is skipped.
int
TriggerCameraToGrid(int32 cam)
{
	switch (cam) {
		case 0: return CAM_FRONT;
		case 6: return CAM_BACK;
		case 5: return CAM_RIGHT;	// best guess - confirm with user
		case 7: return CAM_LEFT;	// best guess
		default: return -1;
	}
}


const char*
TriggerCameraLabel(int32 cam)
{
	int g = TriggerCameraToGrid(cam);
	if (g >= 0) return CameraName((CameraId)g);
	return "(unknown)";
}


// Accepts "YYYY-MM-DDTHH:MM:SS" or "YYYY-MM-DD_HH-MM-SS".
int64
ParseTeslaTimestamp(const char* s)
{
	if (s == NULL || strlen(s) < 19) return -1;
	struct tm t;
	memset(&t, 0, sizeof(t));
	int y, mo, d, h, mi, se;
	// Either separator works because we parse char-by-char.
	if (sscanf(s, "%d-%d-%d%*c%d%*c%d%*c%d",
			&y, &mo, &d, &h, &mi, &se) != 6)
		return -1;
	t.tm_year = y - 1900;
	t.tm_mon  = mo - 1;
	t.tm_mday = d;
	t.tm_hour = h;
	t.tm_min  = mi;
	t.tm_sec  = se;
	t.tm_isdst = -1;
	return (int64)mktime(&t);
}


bigtime_t
Event::ClipStartMicros(size_t i) const
{
	if (clips.empty() || i >= clips.size()) return 0;
	int64 t0 = ParseTeslaTimestamp(clips[0].time.String());
	int64 ti = ParseTeslaTimestamp(clips[i].time.String());
	if (t0 < 0 || ti < 0) return 0;
	return (bigtime_t)(ti - t0) * 1000000LL;
}


bigtime_t
Event::ClipNominalDurationMicros(size_t i) const
{
	if (i + 1 < clips.size()) {
		int64 a = ParseTeslaTimestamp(clips[i].time.String());
		int64 b = ParseTeslaTimestamp(clips[i + 1].time.String());
		if (a >= 0 && b > a)
			return (bigtime_t)(b - a) * 1000000LL;
	}
	return 60LL * 1000000LL;	// Tesla clips are 1 minute by convention
}


bigtime_t
Event::AlarmGlobalMicros() const
{
	if (clips.empty() || timestamp.IsEmpty()) return -1;
	int64 t0 = ParseTeslaTimestamp(clips[0].time.String());
	int64 te = ParseTeslaTimestamp(timestamp.String());
	if (t0 < 0 || te < 0) return -1;
	int64 delta = te - t0;
	if (delta < 0) return -1;
	return (bigtime_t)delta * 1000000LL;
}


int32
Event::FindClipForGlobalTime(bigtime_t t, bigtime_t& localOffset) const
{
	for (size_t i = 0; i < clips.size(); i++) {
		bigtime_t start = ClipStartMicros(i);
		bigtime_t dur   = ClipNominalDurationMicros(i);
		if (t >= start && t < start + dur) {
			localOffset = t - start;
			return (int32)i;
		}
	}
	if (!clips.empty()) {
		localOffset = ClipNominalDurationMicros(clips.size() - 1);
		return (int32)clips.size() - 1;
	}
	localOffset = 0;
	return -1;
}


BString
Event::DisplayLabel() const
{
	BString s;
	s << timestamp;
	if (city.Length() > 0) {
		s << "  -  " << city;
		if (street.Length() > 0)
			s << ", " << street;
	}
	return s;
}


// Minimal JSON value extractor for flat string-only objects like Tesla's
// event.json. Looks for "key" and returns text between the next pair of
// double quotes after the colon.
static bool
ExtractString(const std::string& text, const char* key, BString& out)
{
	std::string needle = std::string("\"") + key + "\"";
	size_t pos = text.find(needle);
	if (pos == std::string::npos)
		return false;
	pos = text.find(':', pos + needle.size());
	if (pos == std::string::npos)
		return false;
	size_t q1 = text.find('"', pos + 1);
	if (q1 == std::string::npos)
		return false;
	size_t q2 = text.find('"', q1 + 1);
	if (q2 == std::string::npos)
		return false;
	out = text.substr(q1 + 1, q2 - q1 - 1).c_str();
	return true;
}


bool
EventScanner::LoadEventJson(const char* path, Event& ev)
{
	BFile file(path, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return false;
	off_t size = 0;
	file.GetSize(&size);
	if (size <= 0 || size > 64 * 1024)
		return false;
	std::string buf(size, '\0');
	if (file.Read(&buf[0], size) != size)
		return false;

	ExtractString(buf, "timestamp", ev.timestamp);
	ExtractString(buf, "city",      ev.city);
	ExtractString(buf, "street",    ev.street);
	ExtractString(buf, "est_lat",   ev.estLat);
	ExtractString(buf, "est_lon",   ev.estLon);
	ExtractString(buf, "reason",    ev.reason);

	BString cam;
	if (ExtractString(buf, "camera", cam))
		ev.camera = atoi(cam.String());
	else
		ev.camera = -1;

	return true;
}


// Tesla filenames: YYYY-MM-DD_HH-MM-SS-<suffix>.mp4 where suffix is
// "front", "back", "left_repeater", "right_repeater". We group by the
// timestamp prefix so each clip has up to 4 files.
static int
SuffixToCamera(const char* s)
{
	if (strcmp(s, "front") == 0)          return CAM_FRONT;
	if (strcmp(s, "back") == 0)           return CAM_BACK;
	if (strcmp(s, "left_repeater") == 0)  return CAM_LEFT;
	if (strcmp(s, "right_repeater") == 0) return CAM_RIGHT;
	return -1;
}


void
EventScanner::CollectClips(const char* dir, Event& ev)
{
	BDirectory d(dir);
	if (d.InitCheck() != B_OK)
		return;

	std::map<std::string, Clip> bucket;

	BEntry entry;
	while (d.GetNextEntry(&entry) == B_OK) {
		if (!entry.IsFile())
			continue;
		char name[B_FILE_NAME_LENGTH];
		entry.GetName(name);
		size_t len = strlen(name);
		if (len < 5 || strcmp(name + len - 4, ".mp4") != 0)
			continue;

		// Strip ".mp4"
		std::string base(name, len - 4);
		// Tesla timestamp = first 19 chars "YYYY-MM-DD_HH-MM-SS"
		if (base.size() < 21 || base[19] != '-')
			continue;
		std::string ts = base.substr(0, 19);
		std::string suffix = base.substr(20);
		int cam = SuffixToCamera(suffix.c_str());
		if (cam < 0)
			continue;

		BPath p;
		entry.GetPath(&p);

		Clip& c = bucket[ts];
		c.time = ts.c_str();
		c.path[cam] = p.Path();
	}

	for (auto& kv : bucket)
		ev.clips.push_back(kv.second);

	std::sort(ev.clips.begin(), ev.clips.end(),
		[](const Clip& a, const Clip& b) {
			return strcmp(a.time.String(), b.time.String()) < 0;
		});
}


bool
EventScanner::Scan(const char* rootDir, std::vector<Event>& out)
{
	BDirectory root(rootDir);
	if (root.InitCheck() != B_OK)
		return false;

	BEntry entry;
	while (root.GetNextEntry(&entry) == B_OK) {
		if (!entry.IsDirectory())
			continue;
		BPath p;
		entry.GetPath(&p);

		Event ev;
		ev.folderPath = p.Path();
		char fname[B_FILE_NAME_LENGTH];
		entry.GetName(fname);
		ev.folderName = fname;
		ev.camera = -1;

		BPath jsonPath(p.Path(), "event.json");
		LoadEventJson(jsonPath.Path(), ev);
		CollectClips(p.Path(), ev);

		if (ev.clips.empty())
			continue;
		out.push_back(ev);
	}

	std::sort(out.begin(), out.end(),
		[](const Event& a, const Event& b) {
			return strcmp(a.folderName.String(), b.folderName.String()) > 0;
		});

	return true;
}
