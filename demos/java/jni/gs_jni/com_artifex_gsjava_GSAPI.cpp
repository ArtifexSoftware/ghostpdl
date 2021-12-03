#include "com_artifex_gsjava_GSAPI.h"

#include <iapi.h>
#include <gdevdsp.h>
#include <string.h>
#include <memory>
#include <assert.h>
#include <string>

#include "jni_util.h"
#include "callbacks.h"
#include "instance_data.h"

using namespace util;

static void *getAsPointer(JNIEnv *env, jobject object, gs_set_param_type type, bool *success);

static void storeDispalyHandle(GSInstanceData *idata);

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1revision
	(JNIEnv *env, jclass, jobject revision, jint len)
{
	if (revision == NULL)
		return throwNullPointerException(env, "Revision object is NULL");
	gsapi_revision_t gsrevision;
	jint code = gsapi_revision(&gsrevision, sizeof(gsapi_revision_t));
	if (code == 0)
	{
		setByteArrayField(env, revision, "product", gsrevision.product);
		setByteArrayField(env, revision, "copyright", gsrevision.copyright);
		setLongField(env, revision, "revision", gsrevision.revision);
		setLongField(env, revision, "revisionDate", gsrevision.revisiondate);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1new_1instance
	(JNIEnv *env, jclass, jobject instance, jlong callerHandle)
{
	if (instance == NULL)
		return throwNullPointerException(env, "LongReference object is NULL");

	GSInstanceData *idata = new GSInstanceData();
	idata->callerHandle = (void *)callerHandle;

	void *gsInstance = NULL;
	int code = gsapi_new_instance(&gsInstance, idata);
	if (code == 0)
		Reference::setValueField(env, instance, toWrapperType(env, (jlong)gsInstance));

	idata->instance = gsInstance;

	putInstanceData(idata);

	return code;
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1delete_1instance
	(JNIEnv *env, jclass, jlong instance)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	callbacks::setJNIEnv(idata, env);
	gsapi_delete_instance((void *)instance);

	deleteDataFromInstance((void *)instance);
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1stdio_1with_1handle
	(JNIEnv *env, jclass, jlong instance, jobject stdIn, jobject stdOut, jobject stdErr, jlong callerHandle)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	idata->stdioHandle = (void *)callerHandle;

	int code = gsapi_set_stdio_with_handle((void *)instance, callbacks::stdInFunction,
		callbacks::stdOutFunction, callbacks::stdErrFunction, idata);
	if (code == 0)
	{
		callbacks::setJNIEnv(idata, env);
		callbacks::setIOCallbacks(idata, stdIn, stdOut, stdErr);
	}

	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1stdio
	(JNIEnv *env, jclass, jlong instance, jobject stdIn, jobject stdOut, jobject stdErr)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	idata->stdioHandle = NULL;

	int code = gsapi_set_stdio_with_handle((void *)instance, callbacks::stdInFunction,
		callbacks::stdOutFunction, callbacks::stdErrFunction, idata);
	if (code == 0)
	{
		callbacks::setJNIEnv(idata, env);
		callbacks::setIOCallbacks(idata, stdIn, stdOut, stdErr);
	}

	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1poll_1with_1handle
	(JNIEnv *env, jclass, jlong instance, jobject poll, jlong callerHandle)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	int code = gsapi_set_poll_with_handle((void *)instance, callbacks::pollFunction, (void *)callerHandle);
	if (code == 0)
	{
		callbacks::setJNIEnv(idata, env);
		callbacks::setPollCallback(idata, poll);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1poll
	(JNIEnv *env, jclass, jlong instance, jobject poll)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	int code = gsapi_set_poll((void *)instance, callbacks::pollFunction);
	if (code == 0)
	{
		callbacks::setJNIEnv(idata, env);
		callbacks::setPollCallback(idata, poll);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1display_1callback
	(JNIEnv *env, jclass, jlong instance, jobject displayCallback)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	if (idata->hasinit && idata->displayCallback)
		storeDispalyHandle(idata);

	display_callback *cb = new display_callback;
	cb->size = sizeof(display_callback);
	cb->version_major = DISPLAY_VERSION_MAJOR;
	cb->version_minor = DISPLAY_VERSION_MINOR;

	cb->display_open = callbacks::display::displayOpenFunction;
	cb->display_preclose = callbacks::display::displayPrecloseFunction;
	cb->display_close = callbacks::display::displayCloseFunction;
	cb->display_presize = callbacks::display::displayPresizeFunction;
	cb->display_size = callbacks::display::displaySizeFunction;
	cb->display_sync = callbacks::display::displaySyncFunction;
	cb->display_page = callbacks::display::displayPageFunction;
	cb->display_update = callbacks::display::displayUpdateFunction;
	cb->display_memalloc = NULL;
	cb->display_memfree = NULL;
	cb->display_separation = callbacks::display::displaySeparationFunction;
	cb->display_adjust_band_height = callbacks::display::displayAdjustBandHeightFunction;
	cb->display_rectangle_request = callbacks::display::displayRectangleRequestFunction;

	int code = gsapi_set_display_callback((void *)instance, cb);
	if (code == 0)
	{
		callbacks::setJNIEnv(idata, env);
		callbacks::setDisplayCallback(idata, displayCallback);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1register_1callout
	(JNIEnv *env, jclass, jlong instance, jobject callout, jlong calloutHandle)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	int code = gsapi_register_callout((void *)instance, callbacks::calloutFunction, (void *)calloutHandle);
	if (code == 0)
	{
		callbacks::setJNIEnv(idata, env);
		callbacks::setCalloutCallback(idata, callout);
	}
	return code;
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1deregister_1callout
	(JNIEnv *env, jclass, jlong instance, jobject callout, jlong calloutHandle)
{
	gsapi_deregister_callout((void *)instance, callbacks::calloutFunction, (void *)calloutHandle);
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1arg_1encoding
	(JNIEnv *env, jclass, jlong instance, jint encoding)
{
	return gsapi_set_arg_encoding((void *)instance, encoding);
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1default_1device_1list
	(JNIEnv *env, jclass, jlong instance, jbyteArray list, jint listlen)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	if (list == NULL)
		return throwNullPointerException(env, "list");

	jboolean isCopy = false;
	callbacks::setJNIEnv(idata, env);
	int code = gsapi_set_default_device_list((void *)instance,
		(const char *)env->GetByteArrayElements(list, &isCopy), listlen);
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1get_1default_1device_1list
	(JNIEnv *env, jclass, jlong instance, jobject list, jobject listlen)
{
	char *clist = NULL;
	int clistlen = 0;
	int code = gsapi_get_default_device_list((void *)instance, &clist, &clistlen);
	if (code == 0)
	{
		if (list)
		{
			jbyteArray arr = env->NewByteArray(clistlen);
			env->SetByteArrayRegion(arr, 0, clistlen, (jbyte *)clist);
			Reference::setValueField(env, list, arr);
			env->DeleteLocalRef(arr);
		}
		if (listlen)
			Reference::setValueField(env, listlen, toWrapperType(env, (jint)clistlen));
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1init_1with_1args
	(JNIEnv *env, jclass, jlong instance, jint argc, jobjectArray argv)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	if (argv == NULL)
		return throwNullPointerException(env, "argv");

	char **cargv = jbyteArray2DToCharArray(env, argv);

	callbacks::setJNIEnv(idata, env);
	int code = gsapi_init_with_args((void *)instance, argc, cargv);
	delete2DByteArray(argc, cargv);

	if (code == 0)
	{
		idata->hasinit = true;
		storeDispalyHandle(idata);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1begin
	(JNIEnv *env, jclass, jlong instance, jint userErrors, jobject pExitCode)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	int exitCode;
	callbacks::setJNIEnv(idata, env);
	int code = gsapi_run_string_begin((void *)instance, userErrors, &exitCode);
	if (pExitCode)
		Reference::setValueField(env, pExitCode, toWrapperType(env, (jint)exitCode));
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1continue
	(JNIEnv *env, jclass, jlong instance, jbyteArray str, jint length, jint userErrors, jobject pExitCode)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	if (str == NULL)
		return throwNullPointerException(env, "str");

	jboolean copy = false;
	int exitCode;
	const char *cstring = (const char *)env->GetByteArrayElements(str, &copy);
	callbacks::setJNIEnv(idata, env);
	int code = gsapi_run_string_continue((void *)instance, cstring, length, userErrors, &exitCode);
	if (pExitCode)
		Reference::setValueField(env, pExitCode, toWrapperType(env, (jint)exitCode));
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1end
	(JNIEnv *env, jclass, jlong instance, jint userErrors, jobject pExitCode)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	int exitCode;
	callbacks::setJNIEnv(idata, env);
	int code = gsapi_run_string_end((void *)instance, userErrors, &exitCode);
	if (pExitCode)
		Reference::setValueField(env, pExitCode, toWrapperType(env, (jint)exitCode));
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1with_1length
	(JNIEnv *env, jclass, jlong instance, jbyteArray str, jint length, jint userErrors, jobject pExitCode)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	if (str == NULL)
		return throwNullPointerException(env, "str");

	jboolean copy = false;
	int exitCode;
	const char *cstring = (const char *)env->GetByteArrayElements(str, &copy);
	callbacks::setJNIEnv(idata, env);
	int code = gsapi_run_string_with_length((void *)instance, cstring, length, userErrors, &exitCode);
	if (pExitCode)
		Reference::setValueField(env, pExitCode, toWrapperType(env, (jint)exitCode));
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string
	(JNIEnv *env, jclass, jlong instance, jbyteArray str, jint userErrors, jobject pExitCode)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	if (str == NULL)
		return throwNullPointerException(env, "str");

	jboolean copy = false;
	int exitCode;
	const char *cstring = (const char *)env->GetByteArrayElements(str, &copy);
	callbacks::setJNIEnv(idata, env);
	int code = gsapi_run_string((void *)instance, cstring, userErrors, &exitCode);
	if (pExitCode)
		Reference::setValueField(env, pExitCode, toWrapperType(env, (jint)exitCode));
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1file
	(JNIEnv *env, jclass, jlong instance, jbyteArray fileName, jint userErrors, jobject pExitCode)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	if (fileName == NULL)
		return throwNullPointerException(env, "fileName");

	jboolean copy = false;
	int exitCode;
	const char *cstring = (const char *)env->GetByteArrayElements(fileName, &copy);
	callbacks::setJNIEnv(idata, env);
	int code = gsapi_run_file((void *)instance, cstring, userErrors, &exitCode);
	if (pExitCode)
		Reference::setValueField(env, pExitCode, toWrapperType(env, (jint)exitCode));
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1exit
	(JNIEnv *env, jclass, jlong instance)
{
	return gsapi_exit((void *)instance);
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1param
	(JNIEnv *env, jclass, jlong instance, jbyteArray param, jobject value, jint paramType)
{
	GSInstanceData *idata = findDataFromInstance((void *)instance);
	assert(idata);

	if (!param)
		return throwNullPointerException(env, "param");

	gs_set_param_type type = (gs_set_param_type)paramType;
	bool paramSuccess;
	void *data = getAsPointer(env, value, type, &paramSuccess);
	if (!paramSuccess)
	{
		throwIllegalArgumentException(env, "paramType");
		return -1;
	}

	jboolean copy = false;
	const char *cstring = (const char *)env->GetByteArrayElements(param, &copy);

	callbacks::setJNIEnv(idata, env);
	int code = gsapi_set_param((void *)instance, cstring, data, type);
	free(data);

	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1get_1param
	(JNIEnv *env, jclass, jlong instance, jbyteArray param, jlong value, jint paramType)
{
	if (!param)
	{
		throwNullPointerException(env, "paramType");
		return -1;
	}

	jboolean copy = false;
	const char *cstring = (const char *)env->GetByteArrayElements(param, &copy);

	int ret = gsapi_get_param((void *)instance, cstring, (void *)value, (gs_set_param_type)paramType);

	return ret;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1get_1param_1once
	(JNIEnv *env, jclass, jlong instance, jbyteArray param, jobject value, jint paramType)
{
	jboolean copy = false;
	const char *cstring = (const char *)env->GetByteArrayElements(param, &copy);

	int bytes = gsapi_get_param((void *)instance, cstring, NULL, (gs_set_param_type)paramType);
	if (bytes < 0)
		return bytes;

	char *data = new char[bytes];
	int code = gsapi_get_param((void *)instance, cstring, data, (gs_set_param_type)paramType);
	if (code < 0)
	{
		delete[] data;
		return code;
	}

	int stripped = paramType & ~(gs_spt_more_to_come);
	Reference ref = Reference(env, value);

	jbyteArray arr = NULL;
	const char *str = NULL;
	jsize len = 0;
	switch (stripped)
	{
	case gs_spt_null:
		break;
	case gs_spt_bool:
		ref.set((jboolean)*((int *)data));
		break;
	case gs_spt_int:
		ref.set(*((jint *)data));
		break;
	case gs_spt_float:
		ref.set(*((jfloat *)data));
		break;
	case gs_spt_long:
		ref.set(*((jlong *)data));
		break;
	case gs_spt_i64:
		ref.set((jlong)*((long long *)data));
		break;
	case gs_spt_size_t:
		ref.set((jlong)*((size_t *)data));
		break;
	case gs_spt_name:
	case gs_spt_string:
	case gs_spt_parsed:
		str = (const char *)data;
		len = (jsize)strlen(str) + 1;
		arr = env->NewByteArray(len);
		env->SetByteArrayRegion(arr, 0, len, (const jbyte *)str);
		ref.set(arr);
		break;
	case gs_spt_invalid:
	default:
		throwIllegalArgumentException(env, "paramType");
		delete[] data;
		return -1;
		break;
	}
	delete[] data;
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1enumerate_1params
   (JNIEnv *env, jclass, jlong instance, jobject iter, jobject key, jobject paramType)
{
	if (!iter)
	{
		throwNullPointerException(env, "iterator is NULL");
		return -1;
	}

	Reference iterRef = Reference(env, iter);

	Reference keyRef = Reference(env, key);
	Reference typeRef = Reference(env, paramType);

	void *citer = (void *)iterRef.longValue();

	if (env->ExceptionCheck())
		return -1;

	const char *ckey;
	gs_set_param_type type;

	int code = gsapi_enumerate_params((void *)instance, &citer, &ckey, &type);

	if (code == 0)
	{
		iterRef.set((jlong)citer);

		jsize len = (jsize)strlen(ckey) + 1;
		jbyteArray arr = env->NewByteArray(len);
		env->SetByteArrayRegion(arr, 0, len, (const jbyte *)ckey);
		keyRef.set(arr);
		env->DeleteLocalRef(arr);

		typeRef.set((jint)type);
	}

	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1add_1control_1path
	(JNIEnv *env, jclass, jlong instance, jint type, jbyteArray path)
{
	if (!path)
	{
		throwNullPointerException(env, "path is NULL");
		return -1;
	}

	jboolean copy = false;
	const char *cstring = (const char *)env->GetByteArrayElements(path, &copy);

	int exitCode = gsapi_add_control_path((void *)instance, type, cstring);

	return exitCode;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1remove_1control_1path
(JNIEnv *env, jclass, jlong instance, jint type, jbyteArray path)
{
	if (!path)
	{
		throwNullPointerException(env, "path is NULL");
		return -1;
	}

	jboolean copy = false;
	const char *cstring = (const char *)env->GetByteArrayElements(path, &copy);

	int exitCode = gsapi_remove_control_path((void *)instance, type, cstring);

	return exitCode;
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1purge_1control_1paths
	(JNIEnv *, jclass, jlong instance, jint type)
{
	gsapi_purge_control_paths((void *)instance, type);
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1activate_1path_1control
	(JNIEnv *, jclass, jlong instance, jboolean enable)
{
	gsapi_activate_path_control((void *)instance, enable);
}

JNIEXPORT jboolean JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1is_1path_1control_1active
	(JNIEnv *env, jclass, jlong instance)
{
	return gsapi_is_path_control_active((void *)instance);
}

void *getAsPointer(JNIEnv *env, jobject object, gs_set_param_type type, bool *success)
{
	*success = true;
	void *result = NULL;
	int stripped = type & ~gs_spt_more_to_come;

	jbyteArray arr = NULL;
	jboolean copy = false;
	const char *cstring = NULL;
	jsize len = 0;
	switch (stripped)
	{
	case gs_spt_null:
		return result;
		break;
	case gs_spt_bool:
		result = malloc(sizeof(int));
		if (!result)
		{
			throwAllocationError(env, "getAsPointer");
			return NULL;
		}

		*((int *)result) = (bool)toBoolean(env, object);
		break;
	case gs_spt_int:
		result = malloc(sizeof(int));
		if (!result)
		{
			throwAllocationError(env, "getAsPointer");
			return NULL;
		}

		*((int *)result) = (int)toInt(env, object);
		break;
	case gs_spt_float:
		result = malloc(sizeof(float));
		if (!result)
		{
			throwAllocationError(env, "getAsPointer");
			return NULL;
		}

		*((float *)result) = (float)toFloat(env, object);
		break;
	case gs_spt_long:
	case gs_spt_i64:
		result = malloc(sizeof(long long));
		if (!result)
		{
			throwAllocationError(env, "getAsPointer");
			return NULL;
		}

		*((long long *)result) = (long long)toLong(env, object);
		break;
	case gs_spt_size_t:
		result = malloc(sizeof(size_t));
		if (!result)
		{
			throwAllocationError(env, "getAsPointer");
			return NULL;
		}

		*((size_t *)result) = (size_t)toLong(env, object);
		break;
	case gs_spt_name:
	case gs_spt_string:
	case gs_spt_parsed:
		arr = (jbyteArray)object;
		cstring = (const char *)env->GetByteArrayElements(arr, &copy);
		len = env->GetArrayLength(arr);
		result = malloc(sizeof(char) * len);
		if (!result)
		{
			throwAllocationError(env, "getAsPointer");
			return NULL;
		}

		//((char *)result)[len - 1] = 0;
		memcpy(result, cstring, len);
		break;
	case gs_spt_invalid:
	default:
		*success = false;
		break;
	}

	if (env->ExceptionCheck())
	{
		if (result)
			free(result);
		result = NULL;
		*success = false;
	}
	return result;
}

void storeDispalyHandle(GSInstanceData *idata)
{
	static const char PARAM_NAME[] = "DisplayHandle";

	assert(idata);
	assert(idata->instance);

	char *param = NULL;
	int bytes = gsapi_get_param(idata->instance, PARAM_NAME, NULL, gs_spt_string);
	if (bytes == com_artifex_gsjava_GSAPI_GS_ERROR_UNDEFINED)
		idata->displayCallback = NULL;
	else
	{
		// Parse the DisplayHandle string again

		param = new char[bytes];
		gsapi_get_param(idata->instance, PARAM_NAME, param, gs_spt_string);

		char *toparse = param;

		int radix = 10; // default base 10

		// If there is a # character, we need to change the radix
		char *rend = strchr(param, '#');
		if (rend)
		{
			*rend = 0;
			radix = atoi(param);
			toparse = rend + 1;
		}

		char *end;
		long long val = std::strtoll(toparse, &end, radix);

		idata->displayHandle = (void *)val;

		delete[] param;
	}

	char buf[20]; // 16#[16 hex digits][null terminator]
#if defined(_WIN32)
	sprintf_s(buf, "16#%llx", (long long)idata);
#else
    snprintf(buf, sizeof(buf), "16#%llx", (long long)idata);
#endif
	gsapi_set_param(idata->instance, PARAM_NAME, buf, gs_spt_string);
}
