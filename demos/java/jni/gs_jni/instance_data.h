#pragma once

#include <jni.h>

class GSInstanceData
{
public:
	void *instance = NULL;
	void *callerHandle = NULL;

	JNIEnv *env;

	jobject stdIn = NULL;
	jobject stdOut = NULL;
	jobject stdErr = NULL;

	jobject poll = NULL;

	jobject displayCallback = NULL;

	jobject callout = NULL;
};

GSInstanceData *putInstanceData(GSInstanceData *data);
GSInstanceData *findDataFromInstance(void *instance);
GSInstanceData *findDataFromHandle(void *callerHandle);
void deleteDataFromInstance(void *instance);
void deleteDataFromHandle(void *callerHandle);