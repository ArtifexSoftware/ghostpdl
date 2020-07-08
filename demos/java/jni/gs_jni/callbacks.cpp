#include "callbacks.h"

static JNIEnv *g_env = NULL;
static jobject g_stdIn = NULL;
static jobject g_stdOut = NULL;
static jobject g_stdErr = NULL;

void setIOCallbacks(JNIEnv *env, jobject stdIn, jobject stdOut, jobject stdErr)
{
	g_env = env;
	g_stdIn = stdIn;
	g_stdOut = stdOut;
	g_stdErr = stdErr;
}

int stdInFunction(void *callerHandle, char *buf, int len)
{
	int code = 0;
	if (g_env && g_stdIn)
	{
		jmethodID method = g_env->GetMethodID(g_env->GetObjectClass(g_stdIn), "onStdIn", "(J[BI)I");
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (signed char *)buf);
		code = g_env->CallIntMethod(g_stdIn, method, (jlong)callerHandle, byteArray, (jint)len);
	}
	return code;
}

int stdOutFunction(void *callerHandle, const char *str, int len)
{
	int code = 0;
	if (g_env && g_stdOut)
	{
		jmethodID method = g_env->GetMethodID(g_env->GetObjectClass(g_stdOut), "onStdOut", "(J[BI)I");
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (const signed char *)str);
		code = g_env->CallIntMethod(g_stdOut, method, (jlong)callerHandle, byteArray, (jint)len);
	}
	return code;
}

int stdErrFunction(void *callerHandle, const char *str, int len)
{
	int code = 0;
	if (g_env && g_stdErr)
	{
		jmethodID method = g_env->GetMethodID(g_env->GetObjectClass(g_stdErr), "onStdErr", "(J[BI)I");
		jbyteArray byteArray = g_env->NewByteArray(len);
		g_env->SetByteArrayRegion(byteArray, 0, len, (const signed char *)str);
		code = g_env->CallIntMethod(g_stdOut, method, (jlong)callerHandle, byteArray, (jint)len);
	}
	return code;
}
