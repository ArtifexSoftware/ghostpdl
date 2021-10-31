#include "com_artifex_gsjava_util_NativePointer.h"

#include <memory>

#include "jni_util.h"

using namespace util;

JNIEXPORT jlong JNICALL Java_com_artifex_gsjava_util_NativePointer_mallocNative
(JNIEnv *env, jclass, jlong size)
{
	void *ptr = malloc((size_t)size);
	if (ptr == NULL)
		return throwAllocationError(env, "malloc");
	return (jlong)ptr;
}

JNIEXPORT jlong JNICALL Java_com_artifex_gsjava_util_NativePointer_callocNative
(JNIEnv *env, jclass, jlong count, jlong size)
{
	void *ptr = calloc(count, (size_t)size);
	if (ptr == NULL)
		return throwAllocationError(env, "calloc");
	return (jlong)ptr;
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_util_NativePointer_freeNative
(JNIEnv *, jclass, jlong block)
{
	free((void *)block);
}

JNIEXPORT jbyteArray JNICALL Java_com_artifex_gsjava_util_NativePointer_byteArrayNative
(JNIEnv *env, jclass, jlong address, jlong len)
{
	jbyteArray array = env->NewByteArray((jsize)len);
	env->SetByteArrayRegion(array, 0, (jsize)len, (const jbyte *)address);
	return array;
}

JNIEXPORT jbyte JNICALL Java_com_artifex_gsjava_util_NativePointer_byteAtNative
(JNIEnv *, jclass, jlong address, jlong index)
{
	return ((jbyte *)address)[index];
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_util_NativePointer_setByteNative
(JNIEnv *, jclass, jlong address, jlong index, jbyte value)
{
	((jbyte *)address)[index] = value;
}

JNIEXPORT jcharArray JNICALL Java_com_artifex_gsjava_util_NativePointer_charArrayNative
(JNIEnv *env, jclass, jlong address, jlong len)
{
	jcharArray array = env->NewCharArray((jsize)len);
	env->SetCharArrayRegion(array, 0, (jsize)len, (const jchar *)address);
	return array;
}

JNIEXPORT jchar JNICALL Java_com_artifex_gsjava_util_NativePointer_charAtNative
(JNIEnv *, jclass, jlong address, jlong index)
{
	return ((jchar *)address)[index];
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_util_NativePointer_setCharNative
(JNIEnv *, jclass, jlong address, jlong index, jchar value)
{
	((jchar *)address)[index] = value;
}

JNIEXPORT jshortArray JNICALL Java_com_artifex_gsjava_util_NativePointer_shortArrayNative
(JNIEnv *env, jclass, jlong address, jlong len)
{
	jshortArray array = env->NewShortArray((jsize)len);
	env->SetShortArrayRegion(array, 0, (jsize)len, (const jshort *)address);
	return array;
}

JNIEXPORT jshort JNICALL Java_com_artifex_gsjava_util_NativePointer_shortAtNative
(JNIEnv *, jclass, jlong address, jlong index)
{
	return ((jshort *)address)[index];
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_util_NativePointer_setShortNative
(JNIEnv *, jclass, jlong address, jlong index, jshort value)
{
	((jshort *)address)[index] = value;
}

JNIEXPORT jintArray JNICALL Java_com_artifex_gsjava_util_NativePointer_intArrayNative
(JNIEnv *env, jclass, jlong address, jlong len)
{
	jintArray array = env->NewIntArray((jsize)len);
	env->SetIntArrayRegion(array, 0, (jsize)len, (const jint *)address);
	return array;
}

JNIEXPORT jint JNICALL Java_com_artifex_gsjava_util_NativePointer_intAtNative
(JNIEnv *, jclass, jlong address, jlong index)
{
	return ((jint *)address)[index];
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_util_NativePointer_setIntNative
(JNIEnv *, jclass, jlong address, jlong index, jint value)
{
	((jint *)address)[index] = value;
}

JNIEXPORT jlongArray JNICALL Java_com_artifex_gsjava_util_NativePointer_longArrayNative
(JNIEnv *env, jclass, jlong address, jlong len)
{
	jlongArray array = env->NewLongArray((jsize)len);
	env->SetLongArrayRegion(array, 0, (jsize)len, (const jlong *)address);
	return array;
}

JNIEXPORT jlong JNICALL Java_com_artifex_gsjava_util_NativePointer_longAtNative
(JNIEnv *, jclass, jlong address, jlong index)
{
	return ((jlong *)address)[index];
}

JNIEXPORT void JNICALL Java_com_artifex_gsjava_util_NativePointer_setLongNative
(JNIEnv *, jclass, jlong address, jlong index, jlong value)
{
	((jlong *)address)[index] = value;
}