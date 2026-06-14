#ifndef TESLA_EVENT_LIST_ITEM_H
#define TESLA_EVENT_LIST_ITEM_H

#include <ListItem.h>
#include <String.h>

class BBitmap;
class BFont;
class BView;


class EventListItem : public BListItem {
public:
							EventListItem(const char* thumbPath,
								const char* line1,
								const char* line2);
	virtual					~EventListItem();

	virtual void			DrawItem(BView* owner, BRect frame, bool complete);
	virtual void			Update(BView* owner, const BFont* font);

private:
			BBitmap*		fThumb;
			BString			fLine1;
			BString			fLine2;
};

#endif
