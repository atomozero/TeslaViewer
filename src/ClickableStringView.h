#ifndef TESLA_CLICKABLE_STRING_VIEW_H
#define TESLA_CLICKABLE_STRING_VIEW_H

#include <StringView.h>


// BStringView subclass that posts a message to its target window when
// clicked, and shows a hand cursor when a click message is set.
class ClickableStringView : public BStringView {
public:
							ClickableStringView(const char* name,
								const char* text);
	virtual					~ClickableStringView();

	virtual void			AttachedToWindow();
	virtual void			MouseDown(BPoint where);
	virtual void			MouseMoved(BPoint where, uint32 code,
								const BMessage* msg);

			void			SetClickMessage(BMessage* msg);

private:
			BMessage*		fClickMessage;
};

#endif
