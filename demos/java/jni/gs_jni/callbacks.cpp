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

void callbacks::setJNIEnv(GSInstanceData *idata, JNIEnv *env)
{
	idata->env = env;
}

void callbacks::setIOCallbacks(GSInstanceData *idata, jobject stdIn, jobject stdOut, jobject stdErr)
{
	if (idata->env)
	{
		if (idata->stdIn)
			idata->env->DeleteGlobalRef(idata->stdIn);

		if (idata->stdOut)
			idata->env->DeleteGlobalRef(idata->stdOut);

		if (idata->stdErr)
			idata->env->DeleteGlobalRef(idata->stdErr);

		idata->stdIn = idata->env->NewGlobalRef(stdIn);
		idata->stdOut = idata->env->NewGlobalRef(stdOut);
		idata->stdErr = idata->env->NewGlobalRef(stdErr);
	}
}

int callbacks::stdInFunction(void *callerHandle, char *buf, int len)
{
	GSInstanceData *gsdata = (GSInstanceData *)callerHandle;
	assert(gsdata);

	if (!gsdata->env || !gsdata->stdErr) return 0;

	jbyteArray byteArray = gsdata->env->NewByteArray(len);
	gsdata->env->SetByteArrayRegion(byteArray, 0, len, (jbyte *)buf);

	jint result = callIntMethod(gsdata->env, gsdata->stdIn, "onStdIn", STDIN_SIG, (jlong)gsdata->stdioHandle, byteArray, (jint)len);
	if (gsdata->env->ExceptionCheck()) return 0;

	jboolean isCopy = JNI_FALSE;
	jbyte *arr = gsdata->env->GetByteArrayElements(byteArray, &isCopy);

	jsize copySize = result < len ? result : len;
	memcpy(buf, arr, copySize);

	return copySize;
}

int callbacks::stdOutFunction(void *callerHandle, const char *str, int len)
{
	GSInstanceData *gsdata = (GSInstanceData *)callerHandle;
	assert(gsdata);

	if (!gsdata->env || !gsdata->stdOut) return len;

	jbyteArray byteArray = gsdata->env->NewByteArray(len);
	gsdata->env->SetByteArrayRegion(byteArray, 0, len, (const jbyte *)str);

	jint result = callIntMethod(gsdata->env, gsdata->stdOut, "onStdOut", STDOUT_SIG, (jlong)gsdata->stdioHandle, byteArray, (jint)len);
	if (gsdata->env->ExceptionCheck()) return 0;

	return result;
}

int callbacks::stdErrFunction(void *callerHandle, const char *str, int len)
{
	GSInstanceData *gsdata = (GSInstanceData *)callerHandle;
	assert(gsdata);

	if (!gsdata->env || !gsdata->stdErr) return len;

	jbyteArray byteArray = gsdata->env->NewByteArray(len);
	gsdata->env->SetByteArrayRegion(byteArray, 0, len, (const jbyte *)str);

	jint result = callIntMethod(gsdata->env, gsdata->stdErr, "onStdErr", STDERR_SIG, (jlong)gsdata->stdioHandle, byteArray, (jint)len);
	if (gsdata->env->ExceptionCheck()) return 0;

	return result;
}

void callbacks::setPollCallback(GSInstanceData *idata, jobject poll)
{
	if (idata->env)
	{
		if (idata->poll)
			idata->env->DeleteGlobalRef(idata->poll);

		idata->poll = idata->env->NewGlobalRef(poll);
	}
}

int callbacks::pollFunction(void *callerHandle)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)callerHandle;
	assert(gsdata);

	if (gsdata->env && gsdata->poll)
	{
		code = callIntMethod(gsdata->env, gsdata->poll, "onPoll", POLL_SIG, (jlong)gsdata->callerHandle);
	}
	return code;
}

void callbacks::setDisplayCallback(GSInstanceData *idata, jobject displayCallback)
{
	if (idata->env)
	{
		if (idata->displayCallback)
		{
			idata->env->DeleteGlobalRef(idata->displayCallback);
			idata->displayCallback = NULL;
		}

		idata->displayCallback = idata->env->NewGlobalRef(displayCallback);
		//g_displayCallback = displayCallback;
	}
}

void callbacks::setCalloutCallback(GSInstanceData *idata, jobject callout)
{
	if (idata->env)
	{
		if (idata->callout)
			idata->env->DeleteGlobalRef(idata->callout);

		idata->callout = idata->env->NewGlobalRef(callout);
	}
}

int callbacks::calloutFunction(void *instance, void *handle, const char *deviceName, int id, int size, void *data)
{
	int code = 0;

	GSInstanceData *gsdata = findDataFromInstance(instance);
	assert(gsdata);

	if (gsdata->env && gsdata->callout)
	{
		jsize len = (jsize)strlen(deviceName);
		jbyteArray array = gsdata->env->NewByteArray(len);
		gsdata->env->SetByteArrayRegion(array, 0, len, (const jbyte *)deviceName);

		// TODO: gsdata->callerHandle is not consistent with the specification for a callout
		code = callIntMethod(gsdata->env, gsdata->callout, "onCallout", "(JJ[BIIJ)I", (jlong)instance, (jlong)gsdata->callerHandle, array, id, size, (jlong)data);
	}
	return code;
}

int callbacks::display::displayOpenFunction(void *handle, void *device)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		jclass clazz = gsdata->env->GetObjectClass(gsdata->displayCallback);
		const char *name = getClassName(gsdata->env, clazz);
		freeClassName(name);
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayOpen", DISPLAY_OPEN_SIG, (jlong)gsdata->displayHandle, (jlong)device);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayPrecloseFunction(void *handle, void *device)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayPreclose", DISPLAY_PRECLOSE_SIG, (jlong)gsdata->displayHandle, (jlong)device);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayCloseFunction(void *handle, void *device)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayClose", DISPLAY_CLOSE_SIG, (jlong)gsdata->displayHandle, (jlong)device);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayPresizeFunction(void *handle, void *device, int width, int height, int raster, unsigned int format)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayPresize", DISPLAY_PRESIZE_SIG, (jlong)gsdata->displayHandle,
			(jlong)device, width, height, raster, (jint)format);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displaySizeFunction(void *handle, void *device, int width, int height, int raster,
	unsigned int format, unsigned char *pimage)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
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

		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplaySize", DISPLAY_SIZE_SIG, (jlong)gsdata->displayHandle,
			(jlong)device, width, height, raster, (jint)format, bytePointer);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displaySyncFunction(void *handle, void *device)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplaySync", DISPLAY_SYNC_SIG, (jlong)gsdata->displayHandle, (jlong)device);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayPageFunction(void *handle, void *device, int copies, int flush)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayPage", DISPLAY_PAGE_SIG, (jlong)gsdata->displayHandle,
			(jlong)device, copies, flush);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayUpdateFunction(void *handle, void *device, int x, int y, int w, int h)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayUpdate", DISPLAY_UPDATE_SIG, (jlong)gsdata->displayHandle,
			(jlong)device, x, y, w, h);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displaySeparationFunction(void *handle, void *device, int component, const char *componentName,
	unsigned short c, unsigned short m, unsigned short y, unsigned short k)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		jsize len = (jsize)strlen(componentName);
		jbyteArray byteArray = gsdata->env->NewByteArray(len);
		gsdata->env->SetByteArrayRegion(byteArray, 0, len, (const jbyte *)componentName);
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplaySeparation", DISPLAY_SEPARATION_SIG, (jlong)gsdata->displayHandle,
			(jlong)device, component, byteArray, c, m, y, k);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayAdjustBandHeightFunction(void *handle, void *device, int bandHeight)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
	assert(gsdata);

	if (gsdata->env && gsdata->displayCallback)
	{
		code = callIntMethod(gsdata->env, gsdata->displayCallback, "onDisplayAdjustBandHeght", DISPLAY_ADJUST_BAND_HEIGHT_SIG,
			(jlong)gsdata->displayHandle, (jlong)device, bandHeight);
		CHECK_AND_RETURN(gsdata->env);
	}
	return code;
}

int callbacks::display::displayRectangleRequestFunction(void *handle, void *device, void **memory, int *ox, int *oy,
	int *raster, int *plane_raster, int *x, int *y, int *w, int *h)
{
	int code = 0;

	GSInstanceData *gsdata = (GSInstanceData *)handle;
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
			(jlong)gsdata->displayHandle,
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
