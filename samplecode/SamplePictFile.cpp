/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SampleCode.h"
#include "SkDumpCanvas.h"
#include "SkView.h"
#include "SkCanvas.h"
#include "SkGradientShader.h"
#include "SkGraphics.h"
#include "SkImageDecoder.h"
#include "SkOSFile.h"
#include "SkPath.h"
#include "SkPicture.h"
#include "SkRandom.h"
#include "SkRegion.h"
#include "SkShader.h"
#include "SkTileGridPicture.h"
#include "SkUtils.h"
#include "SkColorPriv.h"
#include "SkColorFilter.h"
#include "SkTime.h"
#include "SkTypeface.h"
#include "SkXfermode.h"

#include "SkStream.h"
#include "SkSurface.h"
#include "SkXMLParser.h"

class PictFileView : public SampleView {
public:
    PictFileView(const char name[] = NULL)
        : fFilename(name)
        , fBBox(kNo_BBoxType)
        , fTileSize(SkSize::Make(0, 0)) {
        for (int i = 0; i < kBBoxTypeCount; ++i) {
            fPictures[i] = NULL;
        }
    }

    virtual ~PictFileView() {
        for (int i = 0; i < kBBoxTypeCount; ++i) {
            SkSafeUnref(fPictures[i]);
        }
    }

    virtual void onTileSizeChanged(const SkSize &tileSize) SK_OVERRIDE {
        if (tileSize != fTileSize) {
            fTileSize = tileSize;
            SkSafeSetNull(fPictures[kTileGrid_BBoxType]);
        }
    }

protected:
    // overrides from SkEventSink
    virtual bool onQuery(SkEvent* evt) SK_OVERRIDE {
        if (SampleCode::TitleQ(*evt)) {
            SkString name("P:");
            const char* basename = strrchr(fFilename.c_str(), SkPATH_SEPARATOR);
            name.append(basename ? basename+1: fFilename.c_str());
            if (fBBox != kNo_BBoxType) {
                name.append(fBBox == kRTree_BBoxType ? " <bbox: R>" : " <bbox: T>");
            }
            SampleCode::TitleR(evt, name.c_str());
            return true;
        }
        return this->INHERITED::onQuery(evt);
    }

    virtual bool onEvent(const SkEvent& evt) SK_OVERRIDE {
        if (evt.isType("PictFileView::toggleBBox")) {
            fBBox = (BBoxType)((fBBox + 1) % kBBoxTypeCount);
            return true;
        }
        return this->INHERITED::onEvent(evt);
    }

    virtual void onDrawContent(SkCanvas* canvas) SK_OVERRIDE {
        SkASSERT(fBBox < kBBoxTypeCount);
        SkPicture** picture = fPictures + fBBox;

        if (!*picture) {
            *picture = LoadPicture(fFilename.c_str(), fBBox);
        }
        if (*picture) {
            canvas->drawPicture(**picture);
        }
    }

private:
    enum BBoxType {
        kNo_BBoxType,
        kRTree_BBoxType,
        kTileGrid_BBoxType,

        kLast_BBoxType = kTileGrid_BBoxType
    };
    static const int kBBoxTypeCount = kLast_BBoxType + 1;

    SkString    fFilename;
    SkPicture*  fPictures[kBBoxTypeCount];
    BBoxType    fBBox;
    SkSize      fTileSize;

    SkPicture* LoadPicture(const char path[], BBoxType bbox) {
        SkPicture* pic = NULL;

        SkBitmap bm;
        if (SkImageDecoder::DecodeFile(path, &bm)) {
            bm.setImmutable();
            pic = SkNEW(SkPicture);
            SkCanvas* can = pic->beginRecording(bm.width(), bm.height());
            can->drawBitmap(bm, 0, 0, NULL);
            pic->endRecording();
        } else {
            SkFILEStream stream(path);
            if (stream.isValid()) {
                pic = SkPicture::CreateFromStream(&stream);
            } else {
                SkDebugf("coun't load picture at \"path\"\n", path);
            }

            if (false) {
                SkSurface* surf = SkSurface::NewRasterPMColor(pic->width(), pic->height());
                surf->getCanvas()->drawPicture(*pic);
                surf->unref();
            }
            if (false) { // re-record
                SkPicture p2;
                pic->draw(p2.beginRecording(pic->width(), pic->height()));
                p2.endRecording();

                SkString path2(path);
                path2.append(".new.skp");
                SkFILEWStream writer(path2.c_str());
                p2.serialize(&writer);
            }
        }

        if (!pic) {
            return NULL;
        }

        SkPicture* bboxPicture = NULL;
        switch (bbox) {
        case kNo_BBoxType:
            // no bbox playback necessary
            break;
        case kRTree_BBoxType:
            bboxPicture = SkNEW(SkPicture);
            break;
        case kTileGrid_BBoxType: {
            SkASSERT(!fTileSize.isEmpty());
            SkTileGridPicture::TileGridInfo gridInfo;
            gridInfo.fMargin = SkISize::Make(0, 0);
            gridInfo.fOffset = SkIPoint::Make(0, 0);
            gridInfo.fTileInterval = fTileSize.toRound();
            bboxPicture = SkNEW_ARGS(SkTileGridPicture, (pic->width(), pic->height(), gridInfo));
        } break;
        default:
            SkASSERT(false);
        }

        if (bboxPicture) {
            pic->draw(bboxPicture->beginRecording(pic->width(), pic->height(),
                      SkPicture::kOptimizeForClippedPlayback_RecordingFlag));
            bboxPicture->endRecording();
            SkDELETE(pic);
            return bboxPicture;
        }

        return pic;
    }

    typedef SampleView INHERITED;
};

SampleView* CreateSamplePictFileView(const char filename[]);
SampleView* CreateSamplePictFileView(const char filename[]) {
    return new PictFileView(filename);
}

//////////////////////////////////////////////////////////////////////////////

#if 0
static SkView* MyFactory() { return new PictFileView; }
static SkViewRegister reg(MyFactory);
#endif
