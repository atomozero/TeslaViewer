#ifndef TESLA_MAINWINDOW_H
#define TESLA_MAINWINDOW_H

#include <Window.h>
#include <String.h>

#include <vector>

#include "Event.h"
#include "Settings.h"

class BButton;
class BFilePanel;
class BLayoutItem;
class BListView;
class BMenu;
class BMenuBar;
class BMenuField;
class BMenuItem;
class BSlider;
class BStringView;
class BTextControl;
class BView;
class ClickableStringView;
class MapWindow;
class VideoView;
class PlaybackController;
class TimelineSlider;


class MainWindow : public BWindow {
public:
							MainWindow(const char* root);
	virtual					~MainWindow();

	virtual void			MessageReceived(BMessage* msg);
	virtual bool			QuitRequested();
	virtual void			FrameMoved(BPoint origin);
	virtual void			FrameResized(float w, float h);

public:
	// Called by VideoView when the user clicks a camera tile.
			void			ToggleFullscreen(int32 cameraIndex);
			void			GoToAlarm();

protected:
	virtual	void			DispatchMessage(BMessage* msg, BHandler* h);

private:
			void			_BuildUI();
			void			_Rescan();
			void			_LoadEvent(int32 index);
			void			_LoadClipInternal(int32 clipIndex);
			void			_TogglePlay();
			void			_OnTick();
			void			_OnSeekGlobal();
			void			_OnSpeed();
			void			_UpdateInfo();
			void			_UpdateTimeLabel();
			void			_UpdateSliderFromPosition();
			void			_SetActiveCamera(int32 cam);
			bool			_OnKeyDown(BMessage* msg);
			void			_ExportCurrentClip();
			void			_SaveSnapshot();
			void			_OpenFolderDialog();
			void			_SetRoot(const char* path);
			void			_SaveSettings();
			void			_RebuildEventList();
			int32			_SnapshotCamera() const;
			BString			_CurrentWallClock() const;

			bigtime_t		_CurrentGlobalPosition() const;
			bigtime_t		_EventDuration() const;

private:
			Settings			fSettings;
			BFilePanel*			fFolderPanel;
			BMenuBar*			fMenuBar;
			BString				fRoot;
			std::vector<Event>	fEvents;
			std::vector<int32>	fFilteredIndices;
			int32				fCurrentEvent;
			int32				fCurrentClip;

			VideoView*			fCamView[CAM_COUNT];
			BLayoutItem*		fCamItem[CAM_COUNT];
			BView*				fGridContainer;
			BListView*			fEventList;
			BTextControl*		fSearchText;
			BMenu*				fDateMenu;
			BMenuField*			fDateField;
			BButton*			fPlayBtn;
			BButton*			fAlarmBtn;
			BButton*			fExportBtn;
			TimelineSlider*		fSeek;
			BMenuField*			fSpeedField;
			BMenu*				fSpeedMenu;
			BStringView*		fInfoTime;
			ClickableStringView* fInfoLoc;
			BStringView*		fInfoReason;
			BStringView*		fTimeLabel;
			MapWindow*			fMapWindow;

			PlaybackController*	fController;
			bool				fPlaying;
			float				fSpeed;
			bool				fSeeking;
			int32				fFocusedCam;	// -1 = grid mode
};

#endif
