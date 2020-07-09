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
#define DISPLAY_SIZE_SIG "(JJIIII[B)I"
#define DISPLAY_SYNC_SIG "(JJ)I"
#define DISPLAY_PAGE_SIG "(JJIZ)I"
#define DISPLAY_UPDATE_SIG "(JJIIII)I"
// display memalloc
// display memfree
#define DISPLAY_SEPARATION_SIG "(JJI[BSSSS)I"
#define DISPLAY_ADJUST_BAND_HEIGHT_SIG "(JJI)I"
#define DISPLAY_RECTANGLE_REQUEST "(JJLcom/artifex/gsjava/LongReference;JLcom/artifex/gsjava/IntReference;\
Lcom/artifex/gsjava/IntReference;Lcom/artifex/gsjava/IntReference;Lcom/artifex/gsjava/IntReference;\
Lcom/artifex/gsjava/IntReference;Lcom/artifex/gsjava/IntReference;Lcom/artifex/gsjava/IntReference;)I"

static JNIEnv *g_env = NULL;

static jobject g_stdIn = NULL;
static jobject g_stdOut = NULL;
static jobject g_stdErr = NULL;

static jobject g_poll = NULL;

static jobject g_displayCallback = NULL;

void callbacks::setJNIEnv(JNIEnv *env)
{
	g_env = env;
}

void callbacks::setIOCallbacks(jobject stdIn, jobject stdOut, jobject stdErr)
{
	g_stdIn = stdIn;
	g_stdOut = stdOut;
	g_stdErr = stdErr;
}

int callbacks::stdInFunction(void *callerHandle, char *buf, int len)
{
	int code = 0;
	if (g_env && g_stdIn)
	{
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (signed char *)buf);
		code = util::callIntMethod(g_env, g_stdIn, "onStdIn", STDIN_SIG, (jlong)callerHandle, byteArray, (jint)len);
	}
	return code;
}

int callbacks::stdOutFunction(void *callerHandle, const char *str, int len)
{
	int code = 0;
	if (g_env && g_stdOut)
	{
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (const signed char *)str);
		code = util::callIntMethod(g_env, g_stdOut, "onStdOut", STDOUT_SIG, (jlong)callerHandle, byteArray, (jint)len);
	}
	return code;
}

int callbacks::stdErrFunction(void *callerHandle, const char *str, int len)
{
	int code = 0;
	if (g_env && g_stdErr)
	{
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (const signed char *)str);
		code = util::callIntMethod(g_env, g_stdErr, "onStdErr", STDERR_SIG, (jlong)callerHandle, byteArray, (jint)len);
	}
	return code;
}

void callbacks::setPollCallback(jobject poll)
{
	g_poll = poll;
}

int callbacks::pollFunction(void *callerHandle)
{
	int code = 0;
	if (g_env && g_poll)
	{
		code = util::callIntMethod(g_env, g_poll, "onPoll", POLL_SIG, (jlong)callerHandle);
	}
	return code;
}

void callbacks::setDisplayCallback(jobject displayCallback)
{
}

int callbacks::display::displayOpenFunction(void *handle, void *device)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplayOpen", DISPLAY_OPEN_SIG, (jlong)handle, (jlong)device);
	}
	return code;
}

int callbacks::display::displayPrecloseFunction(void *handle, void *device)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplayPreclose", DISPLAY_PRECLOSE_SIG, (jlong)handle, (jlong)device);
	}
	return code;
}

int callbacks::display::displayCloseFunction(void *handle, void *device)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplayClose", DISPLAY_CLOSE_SIG, (jlong)handle, (jlong)device);
	}
	return code;
}

int callbacks::display::displayPresizeFunction(void *handle, void *device, int width, int height, int raster, unsigned int format)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplayPresize", DISPLAY_PRESIZE_SIG, (jlong)handle,
			(jlong)device, width, height, raster, (jint)format);
	}
	return code;
}

int callbacks::display::displaySizeFunction(void *handle, void *device, int width, int height, int raster, unsigned int format, unsigned char *pimage)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		jsize len = width * height * format;
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (signed char *)pimage);
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplaySize", DISPLAY_SIZE_SIG, (jlong)handle,
			(jlong)device, width, height, raster, (jint)format, byteArray);
	}
	return code;
}

int callbacks::display::displaySyncFunction(void *handle, void *device)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplaySync", DISPLAY_SYNC_SIG, (jlong)handle, (jlong)device);
	}
	return code;
}

int callbacks::display::displayPageFunction(void *handle, void *device, int copies, int flush)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplayPage", DISPLAY_PAGE_SIG, (jlong)handle, (jlong)device, copies, flush);
	}
	return code;
}

int callbacks::display::displayUpdateFunction(void *handle, void *device, int x, int y, int w, int h)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplayUpdate", DISPLAY_UPDATE_SIG, (jlong)handle, (jlong)device, x, y, w, h);
	}
	return code;
}

int callbacks::display::displaySeparationFunction(void *handle, void *device, int component, const char *componentName, unsigned short c, unsigned short m, unsigned short y, unsigned short k)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		jsize len = strlen(componentName);
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (signed char *)componentName);
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplaySeparation", DISPLAY_SEPARATION_SIG, (jlong)handle,
			(jlong)device, component, byteArray, c, m, y, k);
	}
	return code;
}

int callbacks::display::displayAdjustBandHeightFunction(void *handle, void *device, int bandHeight)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		code = util::callIntMethod(g_env, g_displayCallback, "onDisplayAdjustBandHeght", DISPLAY_ADJUST_BAND_HEIGHT_SIG,
			(jlong)handle, (jlong)device, bandHeight);
	}
	return code;
}

int callbacks::display::displayRectangleRequestFunction(void *handle, void *device, void **memory, int *ox, int *oy, int *raster, int *plane_raster, int *x, int *y, int *w, int *h)
{
	int code = 0;
	if (g_env && g_displayCallback)
	{
		jobject memoryRef = util::newLongReference(g_env, (jlong)*memory);
	}
	return code;
}
