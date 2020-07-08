#pragma once

#include <jni.h>

void setByteArrayField(JNIEnv *env, jobject object, const char *field, jbyteArray value);
void setByteArrayField(JNIEnv *env, jobject object, const char *field, const char *string);

void setLongField(JNIEnv *env, jobject object, const char *field, jlong value);

jint throwNoClassDefError(JNIEnv *env, const char *message);
jint throwNullPointerException(JNIEnv *env, const char *message);