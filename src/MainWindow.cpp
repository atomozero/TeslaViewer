#include "MainWindow.h"

#include "CameraMappingWindow.h"
#include "ClickableStringView.h"
#include "EventListItem.h"
#include "MapWindow.h"
#include "Messages.h"
#include "PlaybackController.h"
#include "TimelineSlider.h"
#include "VideoView.h"

#include <set>

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <Box.h>
#include <Button.h>
#include <CardLayout.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <GridLayout.h>
#include <GridLayoutBuilder.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <LayoutItem.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Path.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>
#include <Window.h>

#include <ctime>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>


MainWindow::MainWindow(const char* root)
	:
	BWindow(BRect(40, 40, 1200, 740), "Tesla Sentry Viewer",
		B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE | B_AUTO_UPDATE_SIZE_LIMITS),
	fFolderPanel(NULL),
	fMenuBar(NULL),
	fCurrentEvent(-1),
	fCurrentClip(-1),
	fGridContainer(NULL),
	fEventList(NULL),
	fSearchText(NULL),
	fDateMenu(NULL),
	fDateField(NULL),
	fPlayBtn(NULL),
	fAlarmBtn(NULL),
	fExportBtn(NULL),
	fSeek(NULL),
	fSpeedField(NULL),
	fSpeedMenu(NULL),
	fInfoTime(NULL),
	fInfoLoc(NULL),
	fInfoReason(NULL),
	fTimeLabel(NULL),
	fMapWindow(NULL),
	fController(NULL),
	fPlaying(false),
	fSpeed(1.0f),
	fSeeking(false),
	fFocusedCam(-1)
{
	for (int i = 0; i < CAM_COUNT; i++) {
		fCamView[i] = NULL;
		fCamItem[i] = NULL;
	}

	fSettings.Load();
	// Constructor arg overrides saved setting if explicitly provided.
	if (root != NULL && *root != '\0')
		fRoot = root;
	else
		fRoot = fSettings.path;
	fSpeed = fSettings.speed;
	if (fSettings.frameValid) {
		MoveTo(fSettings.frame.LeftTop());
		ResizeTo(fSettings.frame.Width(), fSettings.frame.Height());
	}

	_BuildUI();
	fController = new PlaybackController(this);
	_Rescan();
}


MainWindow::~MainWindow()
{
	delete fController;
	delete fFolderPanel;
}


bool
MainWindow::QuitRequested()
{
	_SaveSettings();
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


void
MainWindow::FrameMoved(BPoint /*origin*/)
{
	fSettings.frame = Frame();
	fSettings.frameValid = true;
}


void
MainWindow::FrameResized(float /*w*/, float /*h*/)
{
	fSettings.frame = Frame();
	fSettings.frameValid = true;
}


void
MainWindow::_SaveSettings()
{
	fSettings.path = fRoot;
	fSettings.speed = fSpeed;
	if (!fSettings.frameValid) fSettings.frame = Frame();
	fSettings.Save();
}


void
MainWindow::_BuildUI()
{
	// --- menu bar -------------------------------------------------
	fMenuBar = new BMenuBar("menuBar");
	BMenu* fileMenu = new BMenu("File");
	fileMenu->AddItem(new BMenuItem("Open folder...",
		new BMessage(kMsgOpenFolderDialog), 'O'));
	fileMenu->AddItem(new BMenuItem("Rescan",
		new BMessage(kMsgRescan), 'R'));
	fileMenu->AddItem(new BMenuItem("Camera mapping...",
		new BMessage(kMsgOpenCameraMap)));
	fileMenu->AddItem(new BMenuItem("Map of events...",
		new BMessage(kMsgOpenMap), 'M'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Snapshot",
		new BMessage(kMsgSnapshot)));
	fileMenu->AddItem(new BMenuItem("Export 4-up...",
		new BMessage(kMsgExport)));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Quit",
		new BMessage(B_QUIT_REQUESTED), 'Q'));
	fMenuBar->AddItem(fileMenu);

	// --- sidebar: search + date filter + event list ---------------
	fSearchText = new BTextControl("searchText", "", "",
		new BMessage(kMsgSearchChanged));
	fSearchText->SetModificationMessage(new BMessage(kMsgSearchChanged));

	fDateMenu = new BMenu("Period");
	fDateMenu->SetRadioMode(true);
	fDateMenu->SetLabelFromMarked(true);
	struct { const char* label; int32 days; } periods[] = {
		{"All time",      0},
		{"Last 7 days",   7},
		{"Last 30 days",  30},
		{"Last 365 days", 365},
	};
	for (size_t i = 0; i < sizeof(periods) / sizeof(periods[0]); i++) {
		BMessage* m = new BMessage(kMsgDateFilterChanged);
		m->AddInt32("days", periods[i].days);
		BMenuItem* it = new BMenuItem(periods[i].label, m);
		if (i == 0) it->SetMarked(true);
		fDateMenu->AddItem(it);
	}
	fDateField = new BMenuField("dateField", "Period:", fDateMenu);

	fEventList = new BListView("eventList", B_SINGLE_SELECTION_LIST);
	fEventList->SetSelectionMessage(new BMessage(kMsgEventSelected));
	BScrollView* listScroll = new BScrollView("eventScroll", fEventList,
		0, false, true);
	listScroll->SetExplicitMinSize(BSize(300, 200));

	BButton* rescanBtn = new BButton("rescan", "Rescan",
		new BMessage(kMsgRescan));

	BBox* sideBox = new BBox("sideBox");
	sideBox->SetLabel("Events");
	BLayoutBuilder::Group<>(sideBox, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_INSETS, B_USE_BIG_INSETS,
			B_USE_SMALL_INSETS, B_USE_SMALL_INSETS)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(new BStringView("searchLabel", "Search:"))
			.Add(fSearchText)
		.End()
		.Add(fDateField)
		.Add(listScroll)
		.Add(rescanBtn);

	// --- video grid (2x2) -----------------------------------------
	fCamView[CAM_FRONT] = new VideoView("camFront", "Front");
	fCamView[CAM_BACK]  = new VideoView("camBack",  "Back");
	fCamView[CAM_LEFT]  = new VideoView("camLeft",  "Left repeater");
	fCamView[CAM_RIGHT] = new VideoView("camRight", "Right repeater");
	for (int i = 0; i < CAM_COUNT; i++)
		fCamView[i]->SetGridIndex(i);

	BGridLayout* gridLayout = new BGridLayout(B_USE_SMALL_SPACING,
		B_USE_SMALL_SPACING);
	fGridContainer = new BView("camGrid", 0, gridLayout);
	fCamItem[CAM_FRONT] = gridLayout->AddView(fCamView[CAM_FRONT], 0, 0);
	fCamItem[CAM_BACK]  = gridLayout->AddView(fCamView[CAM_BACK],  1, 0);
	fCamItem[CAM_LEFT]  = gridLayout->AddView(fCamView[CAM_LEFT],  0, 1);
	fCamItem[CAM_RIGHT] = gridLayout->AddView(fCamView[CAM_RIGHT], 1, 1);

	// --- info bar -------------------------------------------------
	fInfoTime   = new BStringView("infoTime",   "No event selected");
	fInfoLoc    = new ClickableStringView("infoLoc", "");
	fInfoReason = new BStringView("infoReason", "");
	BFont bold(be_bold_font);
	fInfoTime->SetFont(&bold);

	// --- transport bar --------------------------------------------
	fPlayBtn = new BButton("playBtn", "Play", new BMessage(kMsgPlayPause));
	fAlarmBtn = new BButton("alarmBtn", "Go to alarm",
		new BMessage(kMsgGoToAlarm));
	fExportBtn = new BButton("exportBtn", "Export 4-up...",
		new BMessage(kMsgExport));

	BButton* snapshotBtn = new BButton("snapshotBtn", "Snapshot",
		new BMessage(kMsgSnapshot));

	fSeek = new TimelineSlider("seek", new BMessage(kMsgSeek));

	fTimeLabel = new BStringView("timeLabel", "00:00 / 00:00");

	fSpeedMenu = new BMenu("Speed");
	fSpeedMenu->SetRadioMode(true);
	fSpeedMenu->SetLabelFromMarked(true);
	const float speeds[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
	for (size_t i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
		char buf[16];
		sprintf(buf, "%gx", speeds[i]);
		BMessage* m = new BMessage(kMsgSpeedChanged);
		m->AddFloat("speed", speeds[i]);
		BMenuItem* it = new BMenuItem(buf, m);
		if (speeds[i] == fSpeed) it->SetMarked(true);
		fSpeedMenu->AddItem(it);
	}
	fSpeedField = new BMenuField("speedField", "Speed:", fSpeedMenu);

	BView* transport = BLayoutBuilder::Group<>(B_HORIZONTAL, B_USE_SMALL_SPACING)
		.Add(fPlayBtn)
		.Add(fAlarmBtn)
		.Add(fSeek)
		.Add(fTimeLabel)
		.Add(fSpeedField)
		.Add(snapshotBtn)
		.Add(fExportBtn)
		.View();

	// --- right pane -----------------------------------------------
	BView* rightPane = BLayoutBuilder::Group<>(B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(fInfoTime)
		.Add(fInfoLoc)
		.Add(fInfoReason)
		.AddStrut(4)
		.Add(fGridContainer)
		.Add(transport)
		.View();

	// --- root layout ----------------------------------------------
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_SMALL_INSETS)
			.Add(sideBox, 0.25f)
			.Add(rightPane, 0.75f)
		.End();
}


void
MainWindow::_Rescan()
{
	fEvents.clear();
	EventScanner::Scan(fRoot.String(), fEvents);
	_RebuildEventList();
}


void
MainWindow::_RebuildEventList()
{
	while (fEventList->CountItems() > 0)
		delete fEventList->RemoveItem((int32)0);
	fFilteredIndices.clear();

	BString needle;
	if (fSearchText != NULL) needle = fSearchText->Text();
	needle.ToLower();

	int64 cutoff = 0;
	if (fDateMenu != NULL) {
		BMenuItem* m = fDateMenu->FindMarked();
		if (m != NULL && m->Message() != NULL) {
			int32 days = 0;
			m->Message()->FindInt32("days", &days);
			if (days > 0)
				cutoff = (int64)time(NULL) - (int64)days * 86400LL;
		}
	}

	for (size_t i = 0; i < fEvents.size(); i++) {
		const Event& ev = fEvents[i];

		if (needle.Length() > 0) {
			BString hay;
			hay << ev.city << " " << ev.street << " "
				<< ev.reason << " " << ev.folderName;
			hay.ToLower();
			if (hay.FindFirst(needle) < 0) continue;
		}

		if (cutoff > 0 && !ev.timestamp.IsEmpty()) {
			int64 ts = ParseTeslaTimestamp(ev.timestamp.String());
			if (ts > 0 && ts < cutoff) continue;
		}

		BString line1 = ev.timestamp.IsEmpty()
			? ev.folderName : ev.timestamp;
		BString line2;
		if (!ev.city.IsEmpty()) line2 << ev.city;
		if (!ev.street.IsEmpty()) {
			if (!line2.IsEmpty()) line2 << ", ";
			line2 << ev.street;
		}
		BPath thumb(ev.folderPath.String(), "thumb.png");
		fEventList->AddItem(new EventListItem(thumb.Path(),
			line1.String(), line2.String()));
		fFilteredIndices.push_back((int32)i);
	}

	if (fFilteredIndices.empty()) {
		fInfoTime->SetText(fEvents.empty()
			? "No events found" : "No events match the filter");
		fInfoLoc->SetText("");
		fInfoReason->SetText("");
		return;
	}

	fEventList->Select(0);
	_LoadEvent(fFilteredIndices[0]);
}


void
MainWindow::_LoadEvent(int32 index)
{
	if (index < 0 || (size_t)index >= fEvents.size())
		return;
	fCurrentEvent = index;

	Event& ev = fEvents[index];
	_UpdateInfo();

	// Configure slider markers from clip boundaries + alarm.
	bigtime_t total = _EventDuration();
	std::vector<double> bounds;
	if (total > 0) {
		for (size_t i = 1; i < ev.clips.size(); i++) {
			double n = (double)ev.ClipStartMicros(i) / (double)total;
			bounds.push_back(n);
		}
	}
	fSeek->SetClipBoundaries(bounds);

	bigtime_t alarm = ev.AlarmGlobalMicros();
	if (alarm >= 0 && total > 0)
		fSeek->SetAlarmPosition((double)alarm / (double)total);
	else
		fSeek->SetAlarmPosition(-1.0);

	// Pick the clip closest to (but not after) the alarm; load it.
	int32 best = 0;
	if (alarm >= 0) {
		bigtime_t local;
		int32 c = ev.FindClipForGlobalTime(alarm, local);
		if (c >= 0) best = c;
	}
	_LoadClipInternal(best);

	fSeek->SetEnabled(total > 0);
	_UpdateSliderFromPosition();
	_UpdateTimeLabel();
}


void
MainWindow::_LoadClipInternal(int32 clipIndex)
{
	if (fCurrentEvent < 0) return;
	Event& ev = fEvents[fCurrentEvent];
	if (clipIndex < 0 || (size_t)clipIndex >= ev.clips.size()) return;
	fCurrentClip = clipIndex;

	const Clip& c = ev.clips[clipIndex];
	for (int i = 0; i < CAM_COUNT; i++)
		fCamView[i]->LoadClip(c.path[i].String());

	int triggerGrid = fSettings.CameraToGrid(ev.camera);
	for (int i = 0; i < CAM_COUNT; i++)
		fCamView[i]->SetActive(triggerGrid == i);
}


void
MainWindow::GoToAlarm()
{
	if (fCurrentEvent < 0) return;
	bigtime_t alarm = fEvents[fCurrentEvent].AlarmGlobalMicros();
	if (alarm < 0) return;

	bigtime_t local;
	int32 c = fEvents[fCurrentEvent].FindClipForGlobalTime(alarm, local);
	if (c < 0) return;
	if (c != fCurrentClip)
		_LoadClipInternal(c);
	for (int i = 0; i < CAM_COUNT; i++)
		fCamView[i]->SeekToMicros(local);
	_UpdateSliderFromPosition();
	_UpdateTimeLabel();
}


void
MainWindow::ToggleFullscreen(int32 cameraIndex)
{
	if (cameraIndex < 0 || cameraIndex >= CAM_COUNT) return;

	bool goingFullscreen = fFocusedCam != cameraIndex;
	for (int i = 0; i < CAM_COUNT; i++) {
		if (fCamItem[i] == NULL) continue;
		fCamItem[i]->SetVisible(!goingFullscreen || i == cameraIndex);
	}
	fFocusedCam = goingFullscreen ? cameraIndex : -1;
	fGridContainer->InvalidateLayout(true);
}


void
MainWindow::_TogglePlay()
{
	if (fPlaying) {
		fController->Stop();
		fPlaying = false;
		fPlayBtn->SetLabel("Play");
	} else {
		float fps = fCamView[CAM_FRONT]->FrameRate();
		if (fps <= 0) fps = 30.0f;
		fController->Start(fps, fSpeed);
		fPlaying = true;
		fPlayBtn->SetLabel("Pause");
	}
}


void
MainWindow::_OnTick()
{
	bool anyAlive = false;
	for (int i = 0; i < CAM_COUNT; i++) {
		if (fCamView[i]->AdvanceFrame())
			anyAlive = true;
	}

	if (!anyAlive) {
		// End of current clip - try to advance to the next clip.
		if (fCurrentEvent >= 0
			&& (size_t)(fCurrentClip + 1) < fEvents[fCurrentEvent].clips.size()) {
			_LoadClipInternal(fCurrentClip + 1);
			// First frame already loaded; nothing else.
		} else if (fPlaying) {
			_TogglePlay();
		}
	}

	if (!fSeeking)
		_UpdateSliderFromPosition();
	_UpdateTimeLabel();
}


void
MainWindow::_OnSeekGlobal()
{
	if (fCurrentEvent < 0) return;
	fSeeking = true;
	bigtime_t total = _EventDuration();
	if (total <= 0) { fSeeking = false; return; }

	bigtime_t global = (bigtime_t)((double)fSeek->Value()
		* (double)total / 1000.0);

	bigtime_t local;
	int32 c = fEvents[fCurrentEvent].FindClipForGlobalTime(global, local);
	if (c < 0) { fSeeking = false; return; }

	if (c != fCurrentClip)
		_LoadClipInternal(c);

	for (int i = 0; i < CAM_COUNT; i++)
		fCamView[i]->SeekToMicros(local);

	_UpdateTimeLabel();
	fSeeking = false;
}


void
MainWindow::_OnSpeed()
{
	BMenuItem* m = fSpeedMenu->FindMarked();
	if (m == NULL) return;
	BMessage* msg = m->Message();
	if (msg == NULL) return;
	float s = 1.0f;
	msg->FindFloat("speed", &s);
	fSpeed = s;
	fSettings.speed = s;
	fController->SetSpeed(s);
}


void
MainWindow::_OpenFolderDialog()
{
	if (fFolderPanel == NULL) {
		BMessenger m(this);
		fFolderPanel = new BFilePanel(B_OPEN_PANEL, &m, NULL,
			B_DIRECTORY_NODE, false, NULL, NULL, true, true);
	}
	entry_ref ref;
	BEntry entry(fRoot.String(), true);
	if (entry.InitCheck() == B_OK && entry.GetRef(&ref) == B_OK)
		fFolderPanel->SetPanelDirectory(&ref);
	fFolderPanel->Show();
}


void
MainWindow::_SetRoot(const char* path)
{
	if (path == NULL || *path == '\0') return;
	if (fPlaying) _TogglePlay();
	fRoot = path;
	fSettings.path = path;
	fSettings.Save();
	_Rescan();
}


void
MainWindow::_UpdateInfo()
{
	if (fCurrentEvent < 0) return;
	const Event& ev = fEvents[fCurrentEvent];

	BString line1;
	line1 << ev.timestamp;
	if (line1.IsEmpty()) line1 = ev.folderName;
	fInfoTime->SetText(line1.String());

	BString line2;
	if (!ev.city.IsEmpty()) line2 << ev.city;
	if (!ev.street.IsEmpty()) {
		if (!line2.IsEmpty()) line2 << ", ";
		line2 << ev.street;
	}
	if (!ev.estLat.IsEmpty() && !ev.estLon.IsEmpty()) {
		if (!line2.IsEmpty()) line2 << "  -  ";
		line2 << "GPS " << ev.estLat << ", " << ev.estLon;

		BMessage* click = new BMessage(kMsgOpenMap);
		click->AddDouble("centerLat", atof(ev.estLat.String()));
		click->AddDouble("centerLon", atof(ev.estLon.String()));
		fInfoLoc->SetClickMessage(click);
	} else {
		fInfoLoc->SetClickMessage(NULL);
	}
	fInfoLoc->SetText(line2.String());

	BString line3 = "Reason: ";
	line3 << (ev.reason.IsEmpty() ? "(unknown)" : ev.reason.String());
	if (ev.camera >= 0) {
		int g = fSettings.CameraToGrid(ev.camera);
		const char* name = (g >= 0 && g < CAM_COUNT)
			? CameraName((CameraId)g) : "(unknown)";
		line3 << "    Trigger camera: " << name;
		line3 << " (id " << ev.camera << ")";
	}
	fInfoReason->SetText(line3.String());
}


static void
FormatTime(bigtime_t us, char* buf, size_t n)
{
	int64 secs = us / 1000000LL;
	int m = (int)(secs / 60);
	int s = (int)(secs % 60);
	snprintf(buf, n, "%02d:%02d", m, s);
}


void
MainWindow::_UpdateTimeLabel()
{
	bigtime_t pos = _CurrentGlobalPosition();
	bigtime_t dur = _EventDuration();
	char a[16], b[16], out[128];
	FormatTime(pos, a, sizeof(a));
	FormatTime(dur, b, sizeof(b));
	BString clock = _CurrentWallClock();
	BString tail;
	if (fCurrentEvent >= 0 && fEvents[fCurrentEvent].clips.size() > 1) {
		tail.SetToFormat("    clip %d/%d",
			(int)fCurrentClip + 1,
			(int)fEvents[fCurrentEvent].clips.size());
	}
	if (clock.Length() > 0) {
		snprintf(out, sizeof(out), "%s / %s    [%s]%s",
			a, b, clock.String(), tail.String());
	} else {
		snprintf(out, sizeof(out), "%s / %s%s", a, b, tail.String());
	}
	fTimeLabel->SetText(out);
}


BString
MainWindow::_CurrentWallClock() const
{
	if (fCurrentEvent < 0 || fCurrentClip < 0) return BString();
	const Event& ev = fEvents[fCurrentEvent];
	if ((size_t)fCurrentClip >= ev.clips.size()) return BString();

	int64 base = ParseTeslaTimestamp(ev.clips[fCurrentClip].time.String());
	if (base < 0) return BString();

	bigtime_t offset = fCamView[CAM_FRONT]->Position();
	time_t now = (time_t)(base + offset / 1000000LL);

	struct tm lt;
	if (localtime_r(&now, &lt) == NULL) return BString();
	char buf[16];
	snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
		lt.tm_hour, lt.tm_min, lt.tm_sec);
	return BString(buf);
}


int32
MainWindow::_SnapshotCamera() const
{
	if (fFocusedCam >= 0) return fFocusedCam;
	if (fCurrentEvent >= 0) {
		int g = TriggerCameraToGrid(fEvents[fCurrentEvent].camera);
		if (g >= 0) return g;
	}
	return CAM_FRONT;
}


void
MainWindow::_SaveSnapshot()
{
	int32 cam = _SnapshotCamera();
	BBitmap* src = fCamView[cam]->CurrentFrame();
	if (src == NULL || !src->IsValid() || fCurrentEvent < 0) {
		(new BAlert("Snapshot", "No frame available.", "OK"))->Go();
		return;
	}

	// Build path: <event folder>/snapshot_<cam>_<wallclock>.png
	BString wall = _CurrentWallClock();
	wall.ReplaceAll(":", "-");
	if (wall.Length() == 0) wall = "frame";

	BString camName(CameraName((CameraId)cam));
	camName.ReplaceAll(" ", "_");

	BPath dir(fEvents[fCurrentEvent].folderPath.String());
	BString path;
	path << dir.Path() << "/snapshot_" << camName << "_" << wall << ".png";

	// Clone the bitmap so BBitmapStream owns its own copy; the live
	// bitmap stays attached to VideoView for ongoing decode.
	BBitmap* copy = new BBitmap(src);
	if (copy == NULL || !copy->IsValid()) {
		delete copy;
		fprintf(stderr, "Snapshot: clone failed\n");
		return;
	}

	BBitmapStream stream(copy);
	BFile outFile(path.String(),
		B_CREATE_FILE | B_WRITE_ONLY | B_ERASE_FILE);
	status_t s = B_ERROR;
	if (outFile.InitCheck() == B_OK) {
		BTranslatorRoster* roster = BTranslatorRoster::Default();
		s = roster->Translate(&stream, NULL, NULL, &outFile,
			B_PNG_FORMAT);
	}
	BBitmap* released = NULL;
	stream.DetachBitmap(&released);
	delete released;

	fprintf(stderr, "Snapshot %s: %s\n",
		s == B_OK ? "OK" : "FAIL", path.String());
}


void
MainWindow::_UpdateSliderFromPosition()
{
	bigtime_t dur = _EventDuration();
	if (dur <= 0) return;
	bigtime_t pos = _CurrentGlobalPosition();
	fSeek->SetValue((int32)((double)pos * 1000.0 / (double)dur));
}


bigtime_t
MainWindow::_CurrentGlobalPosition() const
{
	if (fCurrentEvent < 0 || fCurrentClip < 0) return 0;
	const Event& ev = fEvents[fCurrentEvent];
	bigtime_t clipStart = ev.ClipStartMicros((size_t)fCurrentClip);
	return clipStart + fCamView[CAM_FRONT]->Position();
}


bigtime_t
MainWindow::_EventDuration() const
{
	if (fCurrentEvent < 0) return 0;
	const Event& ev = fEvents[fCurrentEvent];
	if (ev.clips.empty()) return 0;
	size_t last = ev.clips.size() - 1;
	return ev.ClipStartMicros(last) + ev.ClipNominalDurationMicros(last);
}


void
MainWindow::_SetActiveCamera(int32 cam)
{
	if (cam < 0 || cam >= CAM_COUNT) return;
	for (int i = 0; i < CAM_COUNT; i++)
		fCamView[i]->SetActive(i == cam);
}


bool
MainWindow::_OnKeyDown(BMessage* msg)
{
	const char* bytes = NULL;
	int32 modifiers = 0;
	msg->FindString("bytes", &bytes);
	msg->FindInt32("modifiers", &modifiers);

	if (bytes == NULL || *bytes == '\0') return false;

	// Don't steal shortcuts that involve modifier keys (Alt, Ctrl,
	// Cmd) - those belong to menus or the system.
	if ((modifiers & (B_COMMAND_KEY | B_CONTROL_KEY | B_OPTION_KEY)) != 0)
		return false;

	uint8 key = (uint8)bytes[0];

	switch (key) {
		case ' ':
			_TogglePlay();
			return true;

		case 'a': case 'A':
			GoToAlarm();
			return true;

		case 'f': case 'F':
			ToggleFullscreen(fFocusedCam >= 0 ? fFocusedCam : CAM_FRONT);
			return true;

		case '1': case '2': case '3': case '4':
			_SetActiveCamera(key - '1');
			return true;

		case 's': case 'S':
			_SaveSnapshot();
			return true;

		case B_LEFT_ARROW:
		case B_RIGHT_ARROW:
		{
			bigtime_t step = 5000000LL; // 5s
			bigtime_t pos = _CurrentGlobalPosition();
			pos += (key == B_RIGHT_ARROW ? step : -step);
			if (pos < 0) pos = 0;
			bigtime_t dur = _EventDuration();
			if (dur > 0 && pos > dur) pos = dur;
			bigtime_t local;
			int32 c = fEvents[fCurrentEvent]
				.FindClipForGlobalTime(pos, local);
			if (c >= 0) {
				if (c != fCurrentClip) _LoadClipInternal(c);
				for (int i = 0; i < CAM_COUNT; i++)
					fCamView[i]->SeekToMicros(local);
				_UpdateSliderFromPosition();
				_UpdateTimeLabel();
			}
			return true;
		}

		case B_UP_ARROW:
			GoToAlarm();
			return true;

		default:
			return false;
	}
}


void
MainWindow::DispatchMessage(BMessage* msg, BHandler* h)
{
	if (msg->what == B_KEY_DOWN && _OnKeyDown(msg))
		return;
	BWindow::DispatchMessage(msg, h);
}


void
MainWindow::_ExportCurrentClip()
{
	if (fCurrentEvent < 0 || fCurrentClip < 0) return;
	const Clip& c = fEvents[fCurrentEvent].clips[fCurrentClip];
	for (int i = 0; i < CAM_COUNT; i++) {
		if (c.path[i].IsEmpty()) {
			BAlert* a = new BAlert("Export",
				"This clip is missing one of the 4 cameras; cannot export.",
				"OK");
			a->Go();
			return;
		}
	}

	BPath dir(fEvents[fCurrentEvent].folderPath.String());
	BString outPath = dir.Path();
	outPath << "/4up_" << c.time << ".mp4";
	BString logPath = dir.Path();
	logPath << "/4up_" << c.time << ".log";

	// libx264 is not available on Haiku's stock ffmpeg; mpeg4 is always
	// present, plays in any .mp4 player and is decoded by Haiku's Media
	// Kit. We also redirect stderr to a log so failures are diagnosable.
	BString cmd;
	cmd << "ffmpeg -y";
	cmd << " -i '" << c.path[CAM_FRONT] << "'";
	cmd << " -i '" << c.path[CAM_BACK]  << "'";
	cmd << " -i '" << c.path[CAM_LEFT]  << "'";
	cmd << " -i '" << c.path[CAM_RIGHT] << "'";
	cmd << " -filter_complex "
		   "'[0:v]scale=640:480[a];"
		   "[1:v]scale=640:480[b];"
		   "[2:v]scale=640:480[c];"
		   "[3:v]scale=640:480[d];"
		   "[a][b]hstack[t];"
		   "[c][d]hstack[bo];"
		   "[t][bo]vstack[v]'";
	cmd << " -map '[v]' -c:v mpeg4 -q:v 5 ";
	cmd << "'" << outPath << "'";
	cmd << " >'" << logPath << "' 2>&1 &";

	system(cmd.String());

	BString info = "Exporting in background to:\n";
	info << outPath;
	info << "\n\nffmpeg log: " << logPath;
	BAlert* alert = new BAlert("Export", info.String(), "OK");
	alert->Go();
}


void
MainWindow::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case kMsgPlaybackTick:
			_OnTick();
			break;

		case kMsgPlayPause:
			_TogglePlay();
			break;

		case kMsgSeek:
			_OnSeekGlobal();
			break;

		case kMsgSpeedChanged:
			_OnSpeed();
			break;

		case kMsgGoToAlarm:
			GoToAlarm();
			break;

		case kMsgVideoClick:
		{
			int32 grid = -1;
			if (msg->FindInt32("grid", &grid) == B_OK)
				ToggleFullscreen(grid);
			break;
		}

		case kMsgExport:
			_ExportCurrentClip();
			break;

		case kMsgSnapshot:
			_SaveSnapshot();
			break;

		case kMsgEventSelected:
		{
			int32 eventIdx = -1;
			if (msg->FindInt32("eventIndex", &eventIdx) == B_OK
				&& eventIdx >= 0
				&& (size_t)eventIdx < fEvents.size()) {
				if (fPlaying) _TogglePlay();
				// Sync list selection if the event is currently shown
				for (size_t i = 0; i < fFilteredIndices.size(); i++) {
					if (fFilteredIndices[i] == eventIdx) {
						fEventList->Select((int32)i);
						break;
					}
				}
				Activate(true);
				_LoadEvent(eventIdx);
				break;
			}
			int32 sel = fEventList->CurrentSelection();
			if (sel >= 0 && (size_t)sel < fFilteredIndices.size()) {
				if (fPlaying) _TogglePlay();
				_LoadEvent(fFilteredIndices[sel]);
			}
			break;
		}

		case kMsgSearchChanged:
		case kMsgDateFilterChanged:
			if (fPlaying) _TogglePlay();
			_RebuildEventList();
			break;

		case kMsgRescan:
			if (fPlaying) _TogglePlay();
			_Rescan();
			break;

		case kMsgOpenFolderDialog:
			_OpenFolderDialog();
			break;

		case kMsgOpenCameraMap:
		{
			std::set<int32> ids;
			for (const auto& e : fEvents)
				if (e.camera >= 0) ids.insert(e.camera);
			std::vector<int32> idVec(ids.begin(), ids.end());
			CameraMappingWindow* w = new CameraMappingWindow(
				BMessenger(this), &fSettings, idVec);
			w->Show();
			break;
		}

		case kMsgOpenMap:
		{
			if (fMapWindow == NULL) {
				fMapWindow = new MapWindow(&fEvents,
					BMessenger(this));
				fMapWindow->SetCloseNotificationTarget(
					BMessenger(this));
				fMapWindow->Show();
			} else {
				fMapWindow->Activate(true);
			}
			double lat = 0, lon = 0;
			if (msg->FindDouble("centerLat", &lat) == B_OK
				&& msg->FindDouble("centerLon", &lon) == B_OK) {
				fMapWindow->CenterOn(lat, lon, 16);
			}
			break;
		}

		case kMsgMapClosed:
			fMapWindow = NULL;
			break;

		case kMsgCameraMapChanged:
			if (fCurrentEvent >= 0) {
				_UpdateInfo();
				int triggerGrid = fSettings.CameraToGrid(
					fEvents[fCurrentEvent].camera);
				for (int i = 0; i < CAM_COUNT; i++)
					fCamView[i]->SetActive(triggerGrid == i);
			}
			break;

		case B_REFS_RECEIVED:
		{
			entry_ref ref;
			if (msg->FindRef("refs", 0, &ref) == B_OK) {
				BEntry entry(&ref);
				BPath p;
				if (entry.GetPath(&p) == B_OK)
					_SetRoot(p.Path());
			}
			break;
		}

		default:
			BWindow::MessageReceived(msg);
	}
}
