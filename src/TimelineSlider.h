#ifndef TESLA_TIMELINE_SLIDER_H
#define TESLA_TIMELINE_SLIDER_H

#include <Slider.h>
#include <vector>


// BSlider that paints additional markers on the bar:
//  - One red tick for the alarm instant
//  - One thin grey tick per clip boundary
class TimelineSlider : public BSlider {
public:
							TimelineSlider(const char* name,
								BMessage* changeMsg);

	virtual void			DrawBar();

			void			SetAlarmPosition(double normalized);	// 0..1, <0 = hide
			void			SetClipBoundaries(const std::vector<double>& positions);

private:
			double			fAlarmPos;
			std::vector<double>	fBoundaries;
};

#endif
