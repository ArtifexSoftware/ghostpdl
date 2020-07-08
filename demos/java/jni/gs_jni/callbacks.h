#pragma once

#include <jni.h>

void setIOCallbacks(JNIEnv *env, jobject stdIn, jobject stdOut, jobject stdErr);

int stdInFunction(void *callerHandle, char *buf, int len);
int stdOutFunction(void *callerHandle, const char *str, int len);
int stdErrFunction(void *callerHandle, const char *str, int len);