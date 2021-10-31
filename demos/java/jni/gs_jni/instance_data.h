#pragma once

#include <jni.h>

#include "settings.h"

/*
This class stores data about a Ghostscript instance.
*/
class GSInstanceData
{
public:
	void *instance = NULL;			// The Ghostscript instance
	void *callerHandle = NULL;		// The caller handle passed to gsapi_new_instance
	void *stdioHandle = NULL;		// The caller handle passed to gsapi_set_stdio_with_handle

	JNIEnv *env = NULL;				// The JNIEnv which should be used for JNI API calls

	jobject stdIn = NULL;			// The user IStdInFunction class for stdin input
	jobject stdOut = NULL;			// The user IStdOutFunction class for stdout output
	jobject stdErr = NULL;			// The user IStdErrFunction class for stderr output

	jobject poll = NULL;			// The user IPollFunction class

	jobject displayCallback = NULL;	// The user DisplayCallback class

	jobject callout = NULL;			// The user ICalloutFunction class
};

GSInstanceData *putInstanceData(GSInstanceData *data);
GSInstanceData *findDataFromInstance(void *instance);
void deleteDataFromInstance(void *instance);