#pragma once

#include <jni.h>

namespace callbacks
{
	void setJNIEnv(JNIEnv *env);

	void setIOCallbacks(jobject stdIn, jobject stdOut, jobject stdErr);
	int stdInFunction(void *callerHandle, char *buf, int len);
	int stdOutFunction(void *callerHandle, const char *str, int len);
	int stdErrFunction(void *callerHandle, const char *str, int len);

	void setPollCallback(jobject poll);
	int pollFunction(void *callerHandle);

	void setDisplayCallback(jobject displayCallback);

	namespace display
	{
		int displayOpenFunction(void *handle, void *device);
		int displayPrecloseFunction(void *handle, void *device);
		int displayCloseFunction(void *handle, void *device);
		int displayPresizeFunction(void *handle, void *device, int width,
			int height, int raster, unsigned int format);
		int displaySizeFunction(void *handle, void *device, int width,
			int height, int raster, unsigned int format,
			unsigned char *pimage);
		int displaySyncFunction(void *handle, void *device);
		int displayPageFunction(void *handle, void *device, int copies,
			int flush);
		int displayUpdateFunction(void *handle, void *device, int x,
			int y, int w, int h);
		// display_memalloc omitted
		// display_memfree omitted
		int displaySeparationFunction(void *handle, void *device,
			int component, const char *componentName, unsigned short c,
			unsigned short m, unsigned short y, unsigned short k);
		int displayAdjustBandHeightFunction(void *handle, void *device,
			int bandHeight);
		int displayRectangleRequestFunction(void *handle, void *device,
			void **memory, int *ox, int *oy, int *raster, int *plane_raster,
			int *x, int *y, int *w, int *h);
	}
}