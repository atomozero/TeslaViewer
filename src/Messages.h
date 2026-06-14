#ifndef TESLA_MESSAGES_H
#define TESLA_MESSAGES_H

enum {
	kMsgPlaybackTick     = 'tick',
	kMsgPlayPause        = 'plpa',
	kMsgSeek             = 'seek',
	kMsgSpeedChanged     = 'sped',
	kMsgEventSelected    = 'esel',
	kMsgClipSelected     = 'csel',
	kMsgOpenFolder       = 'opfo',
	kMsgRescan           = 'resc',
	kMsgGoToAlarm        = 'gtal',
	kMsgExport           = 'expt',
	kMsgVideoClick       = 'vdcl',
	kMsgSnapshot         = 'snap',
	kMsgOpenFolderDialog = 'opdl',
	kMsgOpenCameraMap    = 'ocmp',
	kMsgCameraMapChanged = 'cmch',
	kMsgSearchChanged    = 'srch',
	kMsgDateFilterChanged= 'dtfl',
	kMsgOpenMap          = 'omap',
	kMsgMapClosed        = 'mclo'
};

#endif
