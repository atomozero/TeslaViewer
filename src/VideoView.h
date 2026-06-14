#ifndef TESLA_VIDEOVIEW_H
#define TESLA_VIDEOVIEW_H

#include <View.h>
#include <String.h>
#include <Locker.h>

class BBitmap;
class BMediaFile;
class BMediaTrack;


class VideoView : public BView {
public:
							VideoView(const char* name, const char* label);
	virtual					~VideoView();

	virtual void			Draw(BRect updateRect);
	virtual void			MessageReceived(BMessage* msg);
	virtual void			MouseDown(BPoint where);

			void			SetGridIndex(int32 i) { fGridIndex = i; }
			int32			GridIndex() const { return fGridIndex; }

	// Loads a new clip. Returns true on success. If path is empty the
	// view shows a "no signal" placeholder.
			bool			LoadClip(const char* path);
			void			Unload();

	// Advance one frame. Returns false if end-of-stream.
			bool			AdvanceFrame();

	// Seek to the given time in microseconds, then read one frame so
	// we have something to display.
			bool			SeekToMicros(bigtime_t t);

			bigtime_t		Duration() const { return fDuration; }
			bigtime_t		Position() const { return fPosition; }
			float			FrameRate() const { return fFrameRate; }
			BBitmap*		CurrentFrame() const { return fBitmap; }

			void			SetActive(bool active);
			bool			IsActive() const { return fActive; }

private:
			bool			_OpenTrack(const char* path);
			void			_CloseTrack();
			void			_DrawPlaceholder(BRect r);
			status_t		_ReadOneFrame(bigtime_t& outTime);

private:
			BString			fLabel;
			BString			fPath;
			BMediaFile*		fFile;
			BMediaTrack*	fTrack;
			BBitmap*		fBitmap;
			uint8*			fStaging;
			size_t			fStagingSize;
			int32			fDecodedBytesPerRow;
			int32			fWidth;
			int32			fHeight;
			float			fFrameRate;
			bigtime_t		fDuration;
			bigtime_t		fPosition;
			bool			fActive;
			int32			fGridIndex;
			BLocker			fLock;
};

#endif
