#include "TimelineSlider.h"

#include <View.h>


TimelineSlider::TimelineSlider(const char* name, BMessage* changeMsg)
	:
	BSlider(name, "", changeMsg, 0, 1000, B_HORIZONTAL),
	fAlarmPos(-1.0)
{
	SetModificationMessage(new BMessage(*changeMsg));
}


void
TimelineSlider::SetAlarmPosition(double n)
{
	fAlarmPos = n;
	Invalidate();
}


void
TimelineSlider::SetClipBoundaries(const std::vector<double>& positions)
{
	fBoundaries = positions;
	Invalidate();
}


void
TimelineSlider::DrawBar()
{
	BSlider::DrawBar();

	BRect bar = BarFrame();

	// Clip boundary ticks (light grey).
	if (!fBoundaries.empty()) {
		SetHighColor(150, 150, 150);
		SetPenSize(1);
		for (double p : fBoundaries) {
			if (p <= 0.0 || p >= 1.0) continue;
			float x = bar.left + (float)(bar.Width() * p);
			StrokeLine(BPoint(x, bar.top - 2),
				BPoint(x, bar.bottom + 2));
		}
	}

	// Alarm tick (red).
	if (fAlarmPos >= 0.0 && fAlarmPos <= 1.0) {
		SetHighColor(220, 40, 40);
		SetPenSize(2);
		float x = bar.left + (float)(bar.Width() * fAlarmPos);
		StrokeLine(BPoint(x, bar.top - 4),
			BPoint(x, bar.bottom + 4));
		SetPenSize(1);
	}
}
