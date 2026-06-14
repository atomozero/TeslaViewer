#ifndef TESLA_CAMERA_MAPPING_WINDOW_H
#define TESLA_CAMERA_MAPPING_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include <vector>

class BMenu;
class BMenuField;
class Settings;


// Modal-ish dialog: for each camera id present in the loaded events,
// a dropdown lets the user pick which grid slot it maps to.
class CameraMappingWindow : public BWindow {
public:
							CameraMappingWindow(BMessenger target,
								Settings* settings,
								const std::vector<int32>& detectedIds);

	virtual void			MessageReceived(BMessage* msg);

private:
			void			_Commit();

private:
			Settings*				fSettings;
			BMessenger				fTarget;
			std::vector<int32>		fIds;
			std::vector<BMenu*>		fMenus;
};

#endif
