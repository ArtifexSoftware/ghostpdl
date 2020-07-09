#include "com_artifex_gsjava_GSAPI.h"

#include <iapi.h>

#include "jni_util.h"
#include "callbacks.h"

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1revision
	(JNIEnv *env, jclass, jobject revision, jint len)
{
	if (revision == NULL)
		return util::throwNullPointerException(env, "Revision object is NULL");
	gsapi_revision_t gsrevision;
	jint code = gsapi_revision(&gsrevision, sizeof(gsapi_revision_t));
	if (code == 0)
	{
		util::setByteArrayField(env, revision, "product", gsrevision.product);
		util::setByteArrayField(env, revision, "copyright", gsrevision.copyright);
		util::setLongField(env, revision, "revision", gsrevision.revision);
		util::setLongField(env, revision, "revisionDate", gsrevision.revisiondate);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1new_1instance
	(JNIEnv *env, jclass, jobject instance, jlong callerHandle)
{
	if (instance == NULL)
		return util::throwNullPointerException(env, "LongReference object is NULL");

	void *gsInstance;
	int code = gsapi_new_instance(&gsInstance, (void *)callerHandle);
	if (code == 0)
		util::setLongField(env, instance, "value", (jlong)gsInstance);
	return code;
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1delete_1instance
	(JNIEnv *, jclass, jlong instance)
{
	gsapi_delete_instance((void *)instance);
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1stdio_1with_1handle
	(JNIEnv *env, jclass, jlong instance, jobject stdIn, jobject stdOut, jobject stdErr, jlong callerHandle)
{
	int code = gsapi_set_stdio_with_handle((void *)instance, callbacks::stdInFunction,
		callbacks::stdOutFunction, callbacks::stdErrFunction, (void *)callerHandle);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setIOCallbacks(stdIn, stdOut, stdErr);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1stdio
	(JNIEnv *env, jclass, jlong instance, jobject stdIn, jobject stdOut, jobject stdErr)
{
	int code = gsapi_set_stdio((void *)instance, callbacks::stdInFunction,
		callbacks::stdOutFunction, callbacks::stdErrFunction);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setIOCallbacks(stdIn, stdOut, stdErr);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1poll_1with_1handle
	(JNIEnv *env, jclass, jlong instance, jobject poll, jlong callerHandle)
{
	int code = gsapi_set_poll_with_handle((void *)instance, callbacks::pollFunction, (void *)callerHandle);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setPollCallback(poll);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1poll
	(JNIEnv *env, jclass, jlong instance, jobject poll)
{
	int code = gsapi_set_poll((void *)instance, callbacks::pollFunction);
	if (code == 0)
	{
		callbacks::setJNIEnv(env);
		callbacks::setPollCallback(poll);
	}
	return code;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1display_1callback
	(JNIEnv *, jclass, jlong, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1register_1callout
	(JNIEnv *, jclass, jlong, jobject, jlong)
{
	return 0;
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1deregister_1callout
	(JNIEnv *, jclass, jlong, jobject, jlong)
{

}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1arg_1encoding
	(JNIEnv *, jclass, jlong, jint)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1set_1default_1device_1list
	(JNIEnv *, jclass, jlong, jbyteArray, jint)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1get_1default_1device_1list
	(JNIEnv *, jclass, jlong, jobject, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1init_1with_1args
	(JNIEnv *, jclass, jlong, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1begin
	(JNIEnv *, jclass, jlong, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1continue
	(JNIEnv *, jclass, jlong, jbyteArray, jint, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1end
	(JNIEnv *, jclass, jlong, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string_1with_1length
	(JNIEnv *, jclass, jlong, jbyteArray, jint, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1string
	(JNIEnv *, jclass, jlong, jbyteArray, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1run_1file
	(JNIEnv *, jclass, jlong, jbyteArray, jint, jobject)
{
	return 0;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_GSAPI_gsapi_1exit
	(JNIEnv *, jclass, jlong)
{
	return 0;
}