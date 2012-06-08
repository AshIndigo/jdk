/*
 * Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <jni.h>

#include "sun_hawt_HaikuWindowSurfaceData.h"

#include "Drawable.h"
#include "SurfaceData.h"

extern "C" {

typedef struct {
   	SurfaceDataOps	sdOps;
	Drawable* 		drawable;
	jint			x;
	jint			y;
	jint			width;
	jint			height;
	jint 			lockflags;
} HaikuWindowSurfaceDataOps;

static jint
HaikuLock(JNIEnv* env, SurfaceDataOps* ops, SurfaceDataRasInfo* rasInfo,
	jint lockflags)
{
	HaikuWindowSurfaceDataOps* operations = (HaikuWindowSurfaceDataOps*)ops;

	// lock now because we're going to be messing with the drawable
	if (!operations->drawable->Lock())
		return SD_FAILURE;

	if ((lockflags & SD_LOCK_RD_WR) != 0 && operations->drawable->IsValid()) {
		SurfaceDataBounds* bounds = &rasInfo->bounds;

		int width = operations->drawable->Width();
		int height = operations->drawable->Height();
		// Could clip away insets here
		if (bounds->x1 < 0)
			bounds->x1 = 0;
		if (bounds->y1 < 0)
			bounds->y1 = 0;
		if (bounds->x2 > width)
			bounds->x2 = width;
		if (bounds->y2 > height)
			bounds->y2 = height;

		if (bounds->x2 > bounds->x1 && bounds->y2 > bounds->y1) {
			operations->lockflags = lockflags;
			return SD_SUCCESS;
		}
	}

	operations->drawable->Unlock();
	return SD_FAILURE;
}

static void
HaikuGetRasInfo(JNIEnv* env, SurfaceDataOps* ops, SurfaceDataRasInfo* rasInfo)
{
	HaikuWindowSurfaceDataOps* operations = (HaikuWindowSurfaceDataOps*)ops;
	Drawable* drawable = operations->drawable;

	if (drawable->IsValid() && (operations->lockflags & SD_LOCK_RD_WR)) {
		rasInfo->rasBase = drawable->Bits();
		rasInfo->pixelStride = drawable->BytesPerPixel();
		rasInfo->pixelBitOffset = 0;
		rasInfo->scanStride = drawable->BytesPerRow();
	} else {
		// fail if they didn't lock or the drawable isn't valid
		rasInfo->rasBase = NULL;
		rasInfo->pixelStride = 0;
		rasInfo->pixelBitOffset = 0;
		rasInfo->scanStride = 0;
	}
}

static void
HaikuRelease(JNIEnv* env, SurfaceDataOps* ops, SurfaceDataRasInfo* rasInfo)
{
}

static void
HaikuUnlock(JNIEnv* env, SurfaceDataOps* ops, SurfaceDataRasInfo* rasInfo)
{
	HaikuWindowSurfaceDataOps* operations = (HaikuWindowSurfaceDataOps*)ops;

	// Must drop the lock before invalidating because otherwise
	// we can deadlock with FrameResized. Invalidate wants
	// the looper lock which FrameResized holds and FrameResized
	// wants (indirectly) the Drawable lock which we hold.
	operations->drawable->Unlock();

	//printf("unlocking drawable: %p\n", operations->drawable);
	// If we were locked for writing the view needs
	// to redraw now.
	if (operations->lockflags & SD_LOCK_WRITE) {
		int x = rasInfo->bounds.x1;
		int y = rasInfo->bounds.y1;
		int w = rasInfo->bounds.x2 - x;
		int h = rasInfo->bounds.y2 - y;
		operations->drawable->Invalidate(Rectangle(x, y, w, h));
	}
}

JNIEXPORT void JNICALL
Java_sun_hawt_HaikuWindowSurfaceData_initIDs(JNIEnv *env, jclass clazz)
{
}

JNIEXPORT void JNICALL
Java_sun_hawt_HaikuWindowSurfaceData_initOps(JNIEnv* env, jobject thiz,
	jlong drawable)
{
	HaikuWindowSurfaceDataOps* operations = (HaikuWindowSurfaceDataOps*)
		SurfaceData_InitOps(env, thiz, sizeof(HaikuWindowSurfaceDataOps));

	operations->sdOps.Lock = &HaikuLock;
	operations->sdOps.GetRasInfo = &HaikuGetRasInfo;
	operations->sdOps.Release = &HaikuRelease;
	operations->sdOps.Unlock = &HaikuUnlock;
	operations->drawable = (Drawable*)drawable;
}

}
