#include "callbacks.h"

#include "jni_util.h"
#include "instance_data.h"

#include <string.h>
#include <unordered_map>
#include <assert.h>

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

#define CHECK_AND_RETURN(E) if (E->ExceptionCheck()) { return -21; }

using namespace util;

void callbacks::setJNIEnv(void *instance, JNIEnv *env)
{
	GSInstanceData *gsdata = findDataFromInstance(instance);
	assert(gsdata);

	gsdata->env = env;
}

void callbacks::setIOCallbacks(void *instance, jobject stdIn, jobject stdOut, jobject stdErr)
{
	GSInstanceData *gsdata = findDataFromInstance(instance);
	assert(gsdata);

	if (gsdata->env)
	{
		if (gsdata->stdIn)
			gsdata->env->DeleteGlobalRef(gsdata->stdIn);

		if (gsdata->stdOut)
			gsdata->env->DeleteGlobalRef(gsdata->stdOut);

		if (gsdata->stdErr)
			gsdata->env->DeleteGlobalRef(gsdata->stdErr);

		gsdata->stdIn = gsdata->env->NewGlobalRef(stdIn);
		gsdata->stdOut = gsdata->env->NewGlobalRef(stdOut);
		gsdata->stdErr = gsdata->env->NewGlobalRef(stdErr);
	}
}

int callbacks::stdInFunction(void *callerHandle, char *buf, int len)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(callerHandle);
	assert(gsdata);

	if (gsdata->env && gsdata->stdIn)
	{
		jbyteArray byteArray = gsdata->env->NewByteArray(len);
		gsdata->env->SetByteArrayRegion(byteArray, 0, len, (jbyte *)buf);
		code = callIntMethod(gsdata->env, gsdata->stdIn, "onStdIn", STDIN_SIG, (jlong)gsdata->stdioHandle, byteArray, (jint)len);
	}
	return code;
}

int callbacks::stdOutFunction(void *callerHandle, const char *str, int len)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(callerHandle);
	assert(gsdata);

	if (gsdata->env && gsdata->stdOut)
	{
		jbyteArray byteArray = gsdata->env->NewByteArray(len);
		gsdata->env->SetByteArrayRegion(byteArray, 0, len, (const jbyte *)str);
		code = callIntMethod(gsdata->env, gsdata->stdOut, "onStdOut", STDOUT_SIG, (jlong)gsdata->stdioHandle, byteArray, (jint)len);
	}
	return code;
}

int callbacks::stdErrFunction(void *callerHandle, const char *str, int len)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(callerHandle);
	assert(gsdata);

	if (gsdata->env && gsdata->stdErr)
	{
		jbyteArray byteArray = gsdata->env->NewByteArray(len);
		gsdata->env->SetByteArrayRegion(byteArray, 0, len, (const jbyte *)str);
		code = callIntMethod(gsdata->env, gsdata->stdErr, "onStdErr", STDERR_SIG, (jlong)gsdata->stdioHandle, byteArray, (jint)len);
	}
	return code;
}

void callbacks::setPollCallback(void *instance, jobject poll)
{
	GSInstanceData *gsdata = findDataFromInstance(instance);
	assert(gsdata);

	if (gsdata->env)
	{
		if (gsdata->poll)
			gsdata->env->DeleteGlobalRef(gsdata->poll);

		gsdata->poll = gsdata->env->NewGlobalRef(poll);
	}
}

int callbacks::pollFunction(void *callerHandle)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(callerHandle);
	assert(gsdata);

	if (gsdata->env && gsdata->poll)
	{
		code = callIntMethod(gsdata->env, gsdata->poll, "onPoll", POLL_SIG, (jlong)gsdata->callerHandle);
	}
	return code;
}

void callbacks::setDisplayCallback(void *instance, jobject displayCallback)
{
	GSInstanceData *gsdata = findDataFromInstance(instance);
	assert(gsdata);

	if (gsdata->env)
	{
		if (gsdata->displayCallback)
		{
			gsdata->env->DeleteGlobalRef(gsdata->displayCallback);
			gsdata->displayCallback = NULL;
		}

		gsdata->displayCallback = gsdata->env->NewGlobalRef(displayCallback);
		//g_displayCallback = displayCallback;
	}
}

void callbacks::setCalloutCallback(void *instance, jobject callout)
{
	GSInstanceData *gsdata = findDataFromInstance(instance);
	assert(gsdata);

	if (gsdata->env)
	{
		if (gsdata->callout)
			gsdata->env->DeleteGlobalRef(gsdata->callout);

		gsdata->callout = gsdata->env->NewGlobalRef(callout);
	}
}

int callbacks::calloutFunction(void *instance, void *handle, const char *deviceName, int id, int size, void *data)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromInstance(instance);
	assert(gsdata);

	if (gsdata->env && gsdata->callout)
	{
		jsize len = strlen(deviceName);
		jbyteArray array = gsdata->env->NewByteArray(len);
		gsdata->env->SetByteArrayRegion(array, 0, len, (const jbyte *)deviceName);
		code = callIntMethod(gsdata->env, gsdata->callout, "onCallout", "(JJ[BIIJ)I", (jlong)instance, (jlong)gsdata->callerHandle, array, id, size, (jlong)data);
	}
	return code;
}

int callbacks::display::displayOpenFunction(void *handle, void *device)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		jclass clazz = gsdata->env->GetObjectClass(gsdata->displayCallback);
		const char *name = getClassName(gsdata->env, clazz);
		freeClassName(name);
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayOpen", DISPLAY_OPEN_SIG, (jlong)gsdata->callerHandle, (jlong)device);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayPrecloseFunction(void *handle, void *device)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayPreclose", DISPLAY_PRECLOSE_SIG, (jlong)gsdata->callerHandle, (jlong)device);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayCloseFunction(void *handle, void *device)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayClose", DISPLAY_CLOSE_SIG, (jlong)gsdata->callerHandle, (jlong)device);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayPresizeFunction(void *handle, void *device, int width, int height, int raster, unsigned int format)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayPresize", DISPLAY_PRESIZE_SIG, (jlong)gsdata->callerHandle,
			(jlong)device, width, height, raster, (jint)format);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displaySizeFunction(void *handle, void *device, int width, int height, int raster,
	unsigned int format, unsigned char *pimage)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		jsize len = height * raster;
		jbyteArray byteArray = gsdata->env->NewByteArray(len);
		gsdata->env->SetByteArrayRegion(byteArray, 0, len, (signed char *)pimage);

		static const char *const bytePointerClassName = "com/artifex/gsjava/util/BytePointer";
		static const char *const nativePointerClassName = "com/artifex/gsjava/util/NativePointer";

		jclass bytePointerClass = gsdata->env->FindClass(bytePointerClassName);
		if (bytePointerClass == NULL)
		{
			throwNoClassDefError(gsdata->env, bytePointerClassName);
			return -21;
		}

		jclass nativePointerClass = gsdata->env->FindClass(nativePointerClassName);
		if (nativePointerClass == NULL)
		{
			throwNoClassDefError(gsdata->env, nativePointerClassName);
			return -21;
		}

		jmethodID constructor = gsdata->env->GetMethodID(bytePointerClass, "<init>", "()V");
		if (constructor == NULL)
		{
			throwNoSuchMethodError(gsdata->env, "com.artifex.gsjava.util.BytePointer.<init>()V");
			return -21;
		}
		jobject bytePointer = gsdata->env->NewObject(bytePointerClass, constructor);

		jfieldID dataPtrID = gsdata->env->GetFieldID(nativePointerClass, "address", "J");
		if (dataPtrID == NULL)
		{
			throwNoSuchFieldError(gsdata->env, "address");
			return -21;
		}

		jfieldID lengthID = gsdata->env->GetFieldID(bytePointerClass, "length", "J");
		if (lengthID == NULL)
		{
			throwNoSuchFieldError(gsdata->env, "length");
			return -21;
		}

		gsdata->env->SetLongField(bytePointer, dataPtrID, (jlong)pimage);
		gsdata->env->SetLongField(bytePointer, lengthID, len);

		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplaySize", DISPLAY_SIZE_SIG, (jlong)gsdata->callerHandle,
			(jlong)device, width, height, raster, (jint)format, bytePointer);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displaySyncFunction(void *handle, void *device)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplaySync", DISPLAY_SYNC_SIG, (jlong)gsdata->callerHandle, (jlong)device);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayPageFunction(void *handle, void *device, int copies, int flush)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayPage", DISPLAY_PAGE_SIG, (jlong)gsdata->callerHandle,
			(jlong)device, copies, flush);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayUpdateFunction(void *handle, void *device, int x, int y, int w, int h)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayUpdate", DISPLAY_UPDATE_SIG, (jlong)gsdata->callerHandle,
			(jlong)device, x, y, w, h);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displaySeparationFunction(void *handle, void *device, int component, const char *componentName,
	unsigned short c, unsigned short m, unsigned short y, unsigned short k)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		jsize len = strlen(componentName);
		jbyteArray byteArray = gsdata->env->NewByteArray(len);
		gsdata->env->SetByteArrayRegion(byteArray, 0, len, (const jbyte *)componentName);
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplaySeparation", DISPLAY_SEPARATION_SIG, (jlong)gsdata->callerHandle,
			(jlong)device, component, byteArray, c, m, y, k);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayAdjustBandHeightFunction(void *handle, void *device, int bandHeight)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayAdjustBandHeght", DISPLAY_ADJUST_BAND_HEIGHT_SIG,
			(jlong)gsdata->callerHandle, (jlong)device, bandHeight);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayRectangleRequestFunction(void *handle, void *device, void **memory, int *ox, int *oy,
	int *raster, int *plane_raster, int *x, int *y, int *w, int *h)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromHandle(handle);
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		Reference memoryRef = Reference(gsdata->env, toWrapperType(gsdata->env, (jlong)*memory));
		Reference oxRef = Reference(gsdata->env, toWrapperType(gsdata->env, (jint)*ox));
		Reference oyRef = Reference(gsdata->env, toWrapperType(gsdata->env, (jint)*oy));
		Reference rasterRef = Reference(gsdata->env, toWrapperType(gsdata->env, (jint)*raster));
		Reference planeRasterRef = Reference(gsdata->env, toWrapperType(gsdata->env, (jint)*plane_raster));
		Reference xRef = Reference(gsdata->env, toWrapperType(gsdata->env, (jint)*x));
		Reference yRef = Reference(gsdata->env, toWrapperType(gsdata->env, (jint)*y));
		Reference wRef = Reference(gsdata->env, toWrapperType(gsdata->env, (jint)*w));
		Reference hRef = Reference(gsdata->env, toWrapperType(gsdata->env, (jint)*h));

		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayRectangleRequest", DISPLAY_RECTANGLE_REQUEST,
			(jlong)gsdata->callerHandle,
			(jlong)device,
			memoryRef.object(),
			oxRef.object(),
			oyRef.object(),
			rasterRef.object(),
			planeRasterRef.object(),
			xRef.object(),
			yRef.object(),
			wRef.object(),
			hRef.object()
		);

		*memory = (void *)memoryRef.longValue();
		*ox = oxRef.intValue();
		*oy = oyRef.intValue();
		*raster = rasterRef.intValue();
		*plane_raster = planeRasterRef.intValue();
		*x = xRef.intValue();
		*y = yRef.intValue();
		*w = wRef.intValue();
		*h = hRef.intValue();

		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}
