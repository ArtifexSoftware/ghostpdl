#pragma once

#include <jni.h>

namespace util
{
	void setByteArrayField(JNIEnv *env, jobject object, const char *field, jbyteArray value);
	void setByteArrayField(JNIEnv *env, jobject object, const char *field, const char *string);

	void setLongField(JNIEnv *env, jobject object, const char *field, jlong value);

	int callIntMethod(JNIEnv *env, jobject object, const char *name, const char* sig, ...);

	jint throwNoClassDefError(JNIEnv *env, const char *message);
	jint throwNullPointerException(JNIEnv *env, const char *message);

	jobject newLongReference(JNIEnv *env);
	jobject newLongReference(JNIEnv *env, jlong value);
}