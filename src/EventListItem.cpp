#include "EventListItem.h"

#include <Bitmap.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <ListView.h>
#include <TranslationUtils.h>
#include <View.h>


static const float kThumbHeight = 56.0f;
static const float kThumbWidth  = 96.0f;
static const float kPad         = 6.0f;


EventListItem::EventListItem(const char* thumbPath,
		const char* line1, const char* line2)
	:
	fThumb(NULL),
	fLine1(line1 ? line1 : ""),
	fLine2(line2 ? line2 : "")
{
	if (thumbPath != NULL && *thumbPath != '\0')
		fThumb = BTranslationUtils::GetBitmapFile(thumbPath);
}


EventListItem::~EventListItem()
{
	delete fThumb;
}


void
EventListItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);
	SetHeight(kThumbHeight + 2 * kPad);
}


void
EventListItem::DrawItem(BView* owner, BRect frame, bool /*complete*/)
{
	rgb_color bg, fgPrimary, fgSecondary;
	if (IsSelected()) {
		bg = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
		fgPrimary = ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR);
		fgSecondary = tint_color(fgPrimary, B_LIGHTEN_1_TINT);
	} else {
		bg = ui_color(B_LIST_BACKGROUND_COLOR);
		fgPrimary = ui_color(B_LIST_ITEM_TEXT_COLOR);
		fgSecondary = tint_color(fgPrimary, B_LIGHTEN_2_TINT);
	}

	owner->SetLowColor(bg);
	owner->FillRect(frame, B_SOLID_LOW);

	BRect imgRect(frame.left + kPad, frame.top + kPad,
		frame.left + kPad + kThumbWidth, frame.top + kPad + kThumbHeight);

	if (fThumb != NULL && fThumb->IsValid()) {
		owner->SetDrawingMode(B_OP_COPY);
		owner->DrawBitmap(fThumb, fThumb->Bounds(), imgRect);
	} else {
		owner->SetHighColor(40, 40, 40);
		owner->FillRect(imgRect);
	}

	owner->SetDrawingMode(B_OP_OVER);

	font_height fh;
	owner->GetFontHeight(&fh);
	float textX = imgRect.right + kPad + 2;
	float lineH = fh.ascent + fh.descent + 2;

	owner->SetHighColor(fgPrimary);
	owner->DrawString(fLine1.String(),
		BPoint(textX, frame.top + kPad + fh.ascent + 2));
	owner->SetHighColor(fgSecondary);
	owner->DrawString(fLine2.String(),
		BPoint(textX, frame.top + kPad + fh.ascent + 2 + lineH));
}
