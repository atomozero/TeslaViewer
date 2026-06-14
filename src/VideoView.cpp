#include "VideoView.h"

#include <Autolock.h>
#include <Bitmap.h>
#include <Entry.h>
#include <Font.h>
#include <MediaFile.h>
#include <MediaTrack.h>
#include <MediaDefs.h>
#include <Window.h>

#include <algorithm>
#include <cstdio>
#include <cstring>


VideoView::VideoView(const char* name, const char* label)
	:
	BView(name, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fLabel(label),
	fFile(NULL),
	fTrack(NULL),
	fBitmap(NULL),
	fStaging(NULL),
	fStagingSize(0),
	fDecodedBytesPerRow(0),
	fWidth(0),
	fHeight(0),
	fFrameRate(30.0f),
	fDuration(0),
	fPosition(0),
	fActive(false),
	fGridIndex(-1),
	fLock("video lock")
{
	SetViewColor(0, 0, 0);
	SetLowColor(0, 0, 0);
	SetExplicitMinSize(BSize(160, 120));
}


VideoView::~VideoView()
{
	_CloseTrack();
}


void
VideoView::_CloseTrack()
{
	BAutolock _(fLock);
	if (fFile != NULL && fTrack != NULL)
		fFile->ReleaseTrack(fTrack);
	fTrack = NULL;
	delete fFile;
	fFile = NULL;
	delete fBitmap;
	fBitmap = NULL;
	delete[] fStaging;
	fStaging = NULL;
	fStagingSize = 0;
	fDecodedBytesPerRow = 0;
	fWidth = fHeight = 0;
	fDuration = fPosition = 0;
	fPath = "";
}


bool
VideoView::_OpenTrack(const char* path)
{
	entry_ref ref;
	if (get_ref_for_path(path, &ref) != B_OK)
		return false;

	fFile = new BMediaFile(&ref);
	if (fFile->InitCheck() != B_OK)
		return false;

	for (int32 i = 0; i < fFile->CountTracks(); i++) {
		BMediaTrack* t = fFile->TrackAt(i);
		media_format f;
		memset(&f, 0, sizeof(f));
		if (t->EncodedFormat(&f) == B_OK && f.IsVideo()) {
			fTrack = t;
			break;
		}
		fFile->ReleaseTrack(t);
	}
	if (fTrack == NULL)
		return false;

	media_format decoded;
	memset(&decoded, 0, sizeof(decoded));
	decoded.type = B_MEDIA_RAW_VIDEO;
	decoded.u.raw_video = media_raw_video_format::wildcard;
	decoded.u.raw_video.display.format = B_RGB32;

	if (fTrack->DecodedFormat(&decoded) != B_OK)
		return false;

	fWidth     = decoded.u.raw_video.display.line_width;
	fHeight    = decoded.u.raw_video.display.line_count;
	fFrameRate = decoded.u.raw_video.field_rate > 0
				? decoded.u.raw_video.field_rate : 30.0f;
	fDuration  = fTrack->Duration();
	fDecodedBytesPerRow = decoded.u.raw_video.display.bytes_per_row;
	if (fDecodedBytesPerRow <= 0)
		fDecodedBytesPerRow = fWidth * 4;

	BRect b(0, 0, fWidth - 1, fHeight - 1);
	fBitmap = new BBitmap(b, B_RGB32);
	if (!fBitmap->IsValid())
		return false;

	// If the decoder writes a different stride than the BBitmap expects we
	// can't decode straight into bitmap memory - use a staging buffer.
	if (fDecodedBytesPerRow != fBitmap->BytesPerRow()) {
		fStagingSize = (size_t)fDecodedBytesPerRow * (size_t)fHeight;
		fStaging = new uint8[fStagingSize];
	}

	return true;
}


status_t
VideoView::_ReadOneFrame(bigtime_t& outTime)
{
	int64 framesRead = 0;
	media_header header;
	void* dst = fStaging != NULL ? (void*)fStaging : fBitmap->Bits();
	status_t s = fTrack->ReadFrames(dst, &framesRead, &header);
	if (s != B_OK || framesRead == 0)
		return s != B_OK ? s : B_ERROR;

	if (fStaging != NULL) {
		// Copy line by line from decoded stride to bitmap stride.
		int32 dstStride = fBitmap->BytesPerRow();
		int32 copyBytes = std::min(dstStride, fDecodedBytesPerRow);
		uint8* d = (uint8*)fBitmap->Bits();
		uint8* s2 = fStaging;
		for (int32 y = 0; y < fHeight; y++) {
			memcpy(d, s2, copyBytes);
			d  += dstStride;
			s2 += fDecodedBytesPerRow;
		}
	}
	outTime = header.start_time;
	return B_OK;
}


bool
VideoView::LoadClip(const char* path)
{
	_CloseTrack();
	if (path == NULL || *path == '\0') {
		Invalidate();
		return false;
	}
	BAutolock _(fLock);
	fPath = path;
	if (!_OpenTrack(path)) {
		_CloseTrack();
		Invalidate();
		return false;
	}

	bigtime_t t = 0;
	if (_ReadOneFrame(t) == B_OK)
		fPosition = t;

	Invalidate();
	return true;
}


void
VideoView::Unload()
{
	_CloseTrack();
	Invalidate();
}


bool
VideoView::AdvanceFrame()
{
	BAutolock _(fLock);
	if (fTrack == NULL || fBitmap == NULL)
		return false;

	bigtime_t t = 0;
	if (_ReadOneFrame(t) != B_OK)
		return false;
	fPosition = t;
	Invalidate();
	return true;
}


bool
VideoView::SeekToMicros(bigtime_t t)
{
	BAutolock _(fLock);
	if (fTrack == NULL || fBitmap == NULL)
		return false;
	if (t < 0) t = 0;
	if (fDuration > 0 && t > fDuration) t = fDuration;

	bigtime_t target = t;
	if (fTrack->SeekToTime(&target) != B_OK)
		return false;
	fPosition = target;

	bigtime_t got = 0;
	if (_ReadOneFrame(got) == B_OK)
		fPosition = got;
	Invalidate();
	return true;
}


void
VideoView::SetActive(bool active)
{
	if (fActive == active) return;
	fActive = active;
	Invalidate();
}


void
VideoView::_DrawPlaceholder(BRect r)
{
	SetHighColor(20, 20, 20);
	FillRect(r);
	SetHighColor(120, 120, 120);
	BString msg = fLabel;
	if (fPath.Length() == 0)
		msg << " - no clip";
	float w = StringWidth(msg.String());
	font_height fh;
	GetFontHeight(&fh);
	float x = r.left + (r.Width() - w) / 2;
	float y = r.top + (r.Height() + fh.ascent) / 2;
	DrawString(msg.String(), BPoint(x, y));
}


void
VideoView::Draw(BRect updateRect)
{
	BRect b = Bounds();

	if (fBitmap == NULL || !fBitmap->IsValid()) {
		_DrawPlaceholder(b);
		return;
	}

	// Fit the frame inside the view preserving aspect.
	float srcW = fWidth;
	float srcH = fHeight;
	float scale = std::min(b.Width() / srcW, b.Height() / srcH);
	float dW = srcW * scale;
	float dH = srcH * scale;
	float dx = b.left + (b.Width() - dW) / 2;
	float dy = b.top + (b.Height() - dH) / 2;
	BRect dst(dx, dy, dx + dW, dy + dH);

	SetHighColor(0, 0, 0);
	FillRect(b);
	DrawBitmap(fBitmap, fBitmap->Bounds(), dst);

	// Label
	SetHighColor(255, 255, 255);
	SetLowColor(0, 0, 0);
	SetDrawingMode(B_OP_OVER);
	DrawString(fLabel.String(), BPoint(b.left + 6, b.top + 14));

	// Active highlight
	if (fActive) {
		SetHighColor(255, 80, 80);
		SetPenSize(3);
		StrokeRect(b.InsetByCopy(2, 2));
		SetPenSize(1);
	}
}


void
VideoView::MessageReceived(BMessage* msg)
{
	BView::MessageReceived(msg);
}


void
VideoView::MouseDown(BPoint /*where*/)
{
	BMessage m('vdcl');	// VideoView click
	m.AddInt32("grid", fGridIndex);
	Window()->PostMessage(&m);
}
