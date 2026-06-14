#ifndef TESLA_PLAYBACK_CONTROLLER_H
#define TESLA_PLAYBACK_CONTROLLER_H

#include <SupportDefs.h>

class BHandler;
class BMessageRunner;
class BMessenger;


// Drives playback by posting tick messages to a handler (the main window)
// at the requested framerate. The window forwards ticks to all VideoViews.
class PlaybackController {
public:
							PlaybackController(BHandler* target);
							~PlaybackController();

			void			Start(float fps, float speed);
			void			Stop();
			bool			IsRunning() const { return fRunning; }

			void			SetSpeed(float speed);
			float			Speed() const { return fSpeed; }

private:
			void			_Reschedule();

private:
			BHandler*		fTarget;
			BMessageRunner*	fRunner;
			float			fFps;
			float			fSpeed;
			bool			fRunning;
};

#endif
