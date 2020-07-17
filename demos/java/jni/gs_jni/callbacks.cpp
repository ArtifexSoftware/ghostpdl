#include "callbacks.h"

#include "jni_util.h"

#include <string.h>

#define STDIN_SIG "(J[BI)I"
#define STDOUT_SIG "(J[BI)I"
#define STDERR_SIG "(J[BI)I"

#define POLL_SIG "(J)I"

#define DISPLAY_OPEN_SIG "(JJ)I"
#define DISPLAY_PRECLOSE_SIG "(JJ)I"
#define DISPLAY_CLOSE_SIG "(JJ)I"
#define DISPLAY_PRESIZE_SIG "(JJIIII)I"
#define DISPLAY_SIZE_SIG "(JJIIIILcom/artifex/gsjava/util/BytePointer;)I"
#define DISPLAY_SYNC_SIG "(JJ)I"
#define DISPLAY_PAGE_SIG "(JJIZ)I"
#define DISPLAY_UPDATE_SIG "(JJIIII)I"
// display memalloc
// display memfree
#define DISPLAY_SEPARATION_SIG "(JJI[BSSSS)I"
#define DISPLAY_ADJUST_BAND_HEIGHT_SIG "(JJI)I"
#define DISPLAY_RECTANGLE_REQUEST "(JJLcom/artifex/gsjava/LongReference;Lcom/artifex/gsjava/IntReference;\
Lcom/artifex/gsjava/IntReference;Lcom/artifex/gsjava/IntReference;Lcom/artifex/gsjava/IntReference;\
Lcom/artifex/gsjava/IntReference;Lcom/artifex/gsjava/IntReference;Lcom/artifex/gsjava/IntReference;\
Lcom/artifex/gsjava/IntReference;)I"

using namespace util;

static JNIEnv *g_env = NULL;

static jobject g_stdIn = NULL;
static jobject g_stdOut = NULL;
static jobject g_stdErr = NULL;

static jobject g_poll = NULL;

static jobject g_displayCallback = NULL;

static jobject g_callout = NULL;

void callbacks::setJNIEnv(JNIEnv *env)
{
	g_env = env;
}

void callbacks::setIOCallbacks(jobject stdIn, jobject stdOut, jobject stdErr)
{
	if (g_env)
	{
		if (g_stdIn)
			g_env->DeleteGlobalRef(g_stdIn);

		if (g_stdOut)
			g_env->DeleteGlobalRef(g_stdOut);

		if (g_stdErr)
			g_env->DeleteGlobalRef(g_stdErr);

		g_stdIn = g_env->NewGlobalRef(stdIn);
		g_stdOut = g_env->NewGlobalRef(stdOut);
		g_stdErr = g_env->NewGlobalRef(stdErr);
	}
}

int callbacks::stdInFunction(void *callerHandle, char *buf, int len)
{
	int code = 0;
	if (g_env && g_stdIn)
	{
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (jbyte *)buf);
		code = callIntMethod(g_env, g_stdIn, "onStdIn", STDIN_SIG, (jlong)callerHandle, byteArray, (jint)len);
	}
	return code;
}

int callbacks::stdOutFunction(void *callerHandle, const char *str, int len)
{
	int code = 0;
	if (g_env && g_stdOut)
	{
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (const jbyte *)str);
		code = callIntMethod(g_env, g_stdOut, "onStdOut", STDOUT_SIG, (jlong)callerHandle, byteArray, (jint)len);
	}
	return code;
}

int callbacks::stdErrFunction(void *callerHandle, const char *str, int len)
{
	int code = 0;
	if (g_env && g_stdErr)
	{
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (const jbyte *)str);
		code = callIntMethod(g_env, g_stdErr, "onStdErr", STDERR_SIG, (jlong)callerHandle, byteArray, (jint)len);
	}
	return code;
}

void callbacks::setPollCallback(jobject poll)
{
	if (g_env)
	{
		if (g_poll)
			g_env->DeleteGlobalRef(g_poll);

		g_poll = g_env->NewGlobalRef(poll);
	}
}

int callbacks::pollFunction(void *callerHandle)
{
	int code = 0;
	if (g_env && g_poll)
	{
		code = callIntMethod(g_env, g_poll, "onPoll", POLL_SIG, (jlong)callerHandle);
	}
	return code;
}

void callbacks::setDisplayCallback(jobject displayCallback)
{
	if (g_env)
	{
		if (g_displayCallback)
			g_env->DeleteGlobalRef(displayCallback);

		g_displayCallback = g_env->NewGlobalRef(displayCallback);
	}
}

void callbacks::setCalloutCallback(jobject callout)
{
	if (g_env)
	{
		if (g_callout)
			g_env->DeleteGlobalRef(g_callout);

		g_callout = g_env->NewGlobalRef(callout);
	}
}

int callbacks::calloutFunction(void *instance, void *handle, const char *deviceName, int id, int size, void *data)
{
	int code = 0;
	if (g_env && g_callout)
	{
		jsize len = strlen(deviceName);
		jbyteArray array = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(array, 0, len, (const jbyte *)deviceName);
		code = callIntMethod(g_env, g_callout, "onCallout", "(JJ[BIIJ)I", (jlong)instance, (jlong)handle, array, id, size, (jlong)data);
	}
	return code;
}

int callbacks::display::displayOpenFunction(void *handle, void *device)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = callIntMethod(g_env, g_displayCallback, "onDisplayOpen", DISPLAY_OPEN_SIG, (jlong)handle, (jlong)device);
	}
	return code;
}

int callbacks::display::displayPrecloseFunction(void *handle, void *device)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = callIntMethod(g_env, g_displayCallback, "onDisplayPreclose", DISPLAY_PRECLOSE_SIG, (jlong)handle, (jlong)device);
	}
	return code;
}

int callbacks::display::displayCloseFunction(void *handle, void *device)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = callIntMethod(g_env, g_displayCallback, "onDisplayClose", DISPLAY_CLOSE_SIG, (jlong)handle, (jlong)device);
	}
	return code;
}

int callbacks::display::displayPresizeFunction(void *handle, void *device, int width, int height, int raster, unsigned int format)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = callIntMethod(g_env, g_displayCallback, "onDisplayPresize", DISPLAY_PRESIZE_SIG, (jlong)handle,
			(jlong)device, width, height, raster, (jint)format);
	}
	return code;
}

int callbacks::display::displaySizeFunction(void *handle, void *device, int width, int height, int raster,
	unsigned int format, unsigned char *pimage)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		jsize len = height * raster;
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (signed char *)pimage);

		static const char *const bytePointerClassName = "com/artifex/gsjava/util/BytePointer";
		static const char *const nativePointerClassName = "com/artifex/gsjava/util/NativePointer";

		jclass bytePointerClass = g_env->FindClass(bytePointerClassName);
		if (bytePointerClass == NULL)
			return throwNoClassDefError(g_env, bytePointerClassName);

		jclass nativePointerClass = g_env->FindClass(nativePointerClassName);
		if (nativePointerClass == NULL)
			return throwNoClassDefError(g_env, nativePointerClassName);



		jmethodID constructor = g_env->GetMethodID(bytePointerClass, "<init>", "()V");
		if (constructor == NULL)
			return throwNoSuchMethodError(g_env, "com.artifex.gsjava.util.BytePointer.<init>()V");
		jobject bytePointer = g_env->NewObject(bytePointerClass, constructor);

		jfieldID dataPtrID = g_env->GetFieldID(nativePointerClass, "address", "J");
		if (dataPtrID == NULL)
			return throwNoSuchFieldError(g_env, "address");

		jfieldID lengthID = g_env->GetFieldID(bytePointerClass, "length", "J");
		if (lengthID == NULL)
			return throwNoSuchFieldError(g_env, "length");

		g_env->SetLongField(bytePointer, dataPtrID, (jlong)pimage);
		g_env->SetLongField(bytePointer, lengthID, len);

		code = callIntMethod(g_env, g_displayCallback, "onDisplaySize", DISPLAY_SIZE_SIG, (jlong)handle,
			(jlong)device, width, height, raster, (jint)format, bytePointer);
	}
	return code;
}

int callbacks::display::displaySyncFunction(void *handle, void *device)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = callIntMethod(g_env, g_displayCallback, "onDisplaySync", DISPLAY_SYNC_SIG, (jlong)handle, (jlong)device);
	}
	return code;
}

int callbacks::display::displayPageFunction(void *handle, void *device, int copies, int flush)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = callIntMethod(g_env, g_displayCallback, "onDisplayPage", DISPLAY_PAGE_SIG, (jlong)handle,
			(jlong)device, copies, flush);
	}
	return code;
}

int callbacks::display::displayUpdateFunction(void *handle, void *device, int x, int y, int w, int h)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = callIntMethod(g_env, g_displayCallback, "onDisplayUpdate", DISPLAY_UPDATE_SIG, (jlong)handle,
			(jlong)device, x, y, w, h);
	}
	return code;
}

int callbacks::display::displaySeparationFunction(void *handle, void *device, int component, const char *componentName,
	unsigned short c, unsigned short m, unsigned short y, unsigned short k)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		jsize len = strlen(componentName);
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (const jbyte *)componentName);
		code = callIntMethod(g_env, g_displayCallback, "onDisplaySeparation", DISPLAY_SEPARATION_SIG, (jlong)handle,
			(jlong)device, component, byteArray, c, m, y, k);
	}
	return code;
}

int callbacks::display::displayAdjustBandHeightFunction(void *handle, void *device, int bandHeight)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = callIntMethod(g_env, g_displayCallback, "onDisplayAdjustBandHeght", DISPLAY_ADJUST_BAND_HEIGHT_SIG,
			(jlong)handle, (jlong)device, bandHeight);
	}
	return code;
}

int callbacks::display::displayRectangleRequestFunction(void *handle, void *device, void **memory, int *ox, int *oy,
	int *raster, int *plane_raster, int *x, int *y, int *w, int *h)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		// All references must be global references to make sure Java doesn't free objects after the call in Java
		LongReference memoryRef = LongReference(g_env, (jlong)*memory).asGlobal();
		IntReference oxRef = IntReference(g_env, *ox).asGlobal();
		IntReference oyRef = IntReference(g_env, *oy).asGlobal();
		IntReference rasterRef = IntReference(g_env, *raster).asGlobal();
		IntReference planeRasterRef = IntReference(g_env, *plane_raster).asGlobal();
		IntReference xRef = IntReference(g_env, *x).asGlobal();
		IntReference yRef = IntReference(g_env, *y).asGlobal();
		IntReference wRef = IntReference(g_env, *w).asGlobal();
		IntReference hRef = IntReference(g_env, *h).asGlobal();

		code = callIntMethod(g_env, g_displayCallback, "onDisplayRectangleRequest", DISPLAY_RECTANGLE_REQUEST,
			(jlong)handle, (jlong)device, memoryRef, oxRef, oyRef, rasterRef, planeRasterRef, xRef, yRef, wRef, hRef);

		*memory = (void *)memoryRef.value();
		*ox = oxRef.value();
		*oy = oyRef.value();
		*raster = rasterRef.value();
		*plane_raster = planeRasterRef.value();
		*x = xRef.value();
		*y = yRef.value();
		*w = wRef.value();
		*h = hRef.value();

		// We don't need references to these objects anymore so delete the globals to allow them to be garbage collected
		memoryRef.deleteGlobal();
		oxRef.deleteGlobal();
		oyRef.deleteGlobal();
		rasterRef.deleteGlobal();
		planeRasterRef.deleteGlobal();
		xRef.deleteGlobal();
		yRef.deleteGlobal();
		wRef.deleteGlobal();
		hRef.deleteGlobal();
	}
	return code;
}
