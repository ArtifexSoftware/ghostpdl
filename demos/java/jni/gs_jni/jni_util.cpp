#include "jni_util.h"

#include <exception>
#include <string.h>
#include <varargs.h>

#define FIELD(E, O, F, S) E->GetFieldID(E->GetObjectClass(O), F, S)

void util::setByteArrayField(JNIEnv *env, jobject object, const char *field, jbyteArray value)
{
    jfieldID fieldID = FIELD(env, object, field, "[B");
    env->SetObjectField(object, fieldID, value);
}

void util::setByteArrayField(JNIEnv *env, jobject object, const char *field, const char *string)
{
    jsize len = (jsize)strlen(string);
    jbyteArray byteArray = env->NewByteArray(len);
    env->SetByteArrayRegion(byteArray, 0, len, (const signed char *)string);
    setByteArrayField(env, object, field, byteArray);
}

void util::setLongField(JNIEnv *env, jobject object, const char *field, jlong value)
{
    jfieldID fieldID = FIELD(env, object, field, "J");
    env->SetLongField(object, fieldID, value);
}

int util::callIntMethod(JNIEnv *env, jobject object, const char *method, const char *sig, ...)
{
    jmethodID methodID = env->GetMethodID(env->GetObjectClass(object), method, sig);
    va_list args;
    int result;
    va_start(args, sig);
    result = env->CallIntMethod(object, methodID, args);
    va_end(args);
    return result;
}

jint util::throwNoClassDefError(JNIEnv *env, const char *message)
{
    jclass exClass;
    static const char *className = "java/lang/NoClassDefFoundError";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        throw std::exception("Failed to find java.lang.NoClassDefFoundError");

    return env->ThrowNew(exClass, message);
}

jint util::throwNullPointerException(JNIEnv *env, const char *message)
{
    jclass exClass;
    static const char *className = "java/lang/NullPointerException";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        return throwNoClassDefError(env, className);

    return env->ThrowNew(exClass, message);
}

jobject util::newLongReference(JNIEnv *env)
{
    jclass lClass;
    static const char *className = "com/arifex/gsjava/util/LongReference";

    lClass = env->FindClass(className);
    if (lClass == NULL)
    {
        throwNoClassDefError(env, className);
        return NULL;
    }

    jmethodID constructor = env->GetMethodID(lClass, "<init>", "()V");

    jobject obj = env->NewObject(lClass, constructor);
    return obj;
}

jobject util::newLongReference(JNIEnv *env, jlong value)
{
    jobject ref = newLongReference(env);
    if (!ref)
        return NULL;
    setLongField(env, ref, "value", value);
    return ref;
}
