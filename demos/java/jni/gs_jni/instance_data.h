#pragma once

#include <jni.h>

#include "settings.h"

class GSInstanceData
{
public:
	void *instance = NULL;
	void *callerHandle = NULL;
	void *stdioHandle = NULL;

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
void deleteDataFromInstance(void *instance);