#include "PlaybackController.h"

#include "Messages.h"

#include <Handler.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Messenger.h>


PlaybackController::PlaybackController(BHandler* target)
	:
	fTarget(target),
	fRunner(NULL),
	fFps(30.0f),
	fSpeed(1.0f),
	fRunning(false)
{
}


PlaybackController::~PlaybackController()
{
	delete fRunner;
}


void
PlaybackController::Start(float fps, float speed)
{
	if (fps > 0) fFps = fps;
	if (speed > 0) fSpeed = speed;
	fRunning = true;
	_Reschedule();
}


void
PlaybackController::Stop()
{
	fRunning = false;
	delete fRunner;
	fRunner = NULL;
}


void
PlaybackController::SetSpeed(float speed)
{
	if (speed <= 0) return;
	fSpeed = speed;
	if (fRunning) _Reschedule();
}


void
PlaybackController::_Reschedule()
{
	delete fRunner;
	fRunner = NULL;
	if (!fRunning) return;

	bigtime_t interval = (bigtime_t)(1000000.0f / (fFps * fSpeed));
	if (interval < 5000) interval = 5000; // floor at 5 ms

	BMessage tick(kMsgPlaybackTick);
	BMessenger m(fTarget);
	fRunner = new BMessageRunner(m, &tick, interval);
}
