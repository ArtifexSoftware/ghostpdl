#include "jni_util.h"

#include <exception>
#include <string.h>

void setByteArrayField(JNIEnv *env, jobject object, const char *field, jbyteArray value)
{
    jfieldID fieldID = env->GetFieldID(env->GetObjectClass(object), field, "[B");
    env->SetObjectField(object, fieldID, value);
}

void setByteArrayField(JNIEnv *env, jobject object, const char *field, const char *string)
{
    jsize len = (jsize)strlen(string);
    jbyteArray byteArray = env->NewByteArray(len);
    env->SetByteArrayRegion(byteArray, 0, len, (const signed char *)string);
    setByteArrayField(env, object, field, byteArray);
}

void setLongField(JNIEnv *env, jobject object, const char *field, jlong value)
{
    jfieldID fieldID = env->GetFieldID(env->GetObjectClass(object), field, "J");
    env->SetLongField(object, fieldID, value);
}


jint throwNoClassDefError(JNIEnv *env, const char *message)
{
    jclass exClass;
    const char *className = "java/lang/NoClassDefFoundError";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        throw std::exception("Failed to find java.lang.NoClassDefFoundError");

    return env->ThrowNew(exClass, message);
}

jint throwNullPointerException(JNIEnv *env, const char *message)
{
    jclass exClass;
    const char *className = "java/lang/NullPointerException";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        return throwNoClassDefError(env, className);

    return env->ThrowNew(exClass, message);
}
