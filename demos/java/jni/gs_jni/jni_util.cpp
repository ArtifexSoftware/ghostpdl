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

jbyteArray util::getByteArrayField(JNIEnv *env, jobject object, const char *field)
{
    jfieldID fieldID = env->GetFieldID(env->GetObjectClass(object), field, "[B");
    return (jbyteArray)env->GetObjectField(object, fieldID);
}

char **util::jbyteArray2DToCharArray(JNIEnv *env, jobjectArray array)
{
    jboolean copy = true;
    jsize len = env->GetArrayLength(array);
    char **result = new char*[len];
    for (jsize i = 0; i < len; i++)
    {
        jbyteArray byteArrayObject = (jbyteArray)env->GetObjectArrayElement(array, i);
        result[i] = (char *)env->GetByteArrayElements(byteArrayObject, &copy);
    }
    return result;
}

void util::delete2DByteArray(int count, char **array)
{
    for (int i = 0; i < count; i++)
    {
        delete[] array[i];
        array[i] = NULL;
    }
    delete[] array;
}

void util::setLongField(JNIEnv *env, jobject object, const char *field, jlong value)
{
    jfieldID fieldID = FIELD(env, object, field, "J");
    env->SetLongField(object, fieldID, value);
}

jlong util::getLongField(JNIEnv *env, jobject object, const char *field)
{
    jfieldID fieldID = FIELD(env, object, field, "J");
    return env->GetLongField(object, fieldID);
}

void util::setIntField(JNIEnv *env, jobject object, const char *field, jint value)
{
    jfieldID fieldID = FIELD(env, object, field, "I");
    env->SetIntField(object, fieldID, value);
}

jint util::getIntField(JNIEnv *env, jobject object, const char *field)
{
    jfieldID fieldID = FIELD(env, object, field, "I");
    return env->GetIntField(object, fieldID);
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
    static const char *const className = "java/lang/NoClassDefFoundError";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        throw std::exception("Failed to find java.lang.NoClassDefFoundError");

    return env->ThrowNew(exClass, message);
}

jint util::throwNullPointerException(JNIEnv *env, const char *message)
{
    jclass exClass;
    static const char *const className = "java/lang/NullPointerException";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        return throwNoClassDefError(env, className);

    return env->ThrowNew(exClass, message);
}

util::LongReference::LongReference(JNIEnv *env) : LongReference(env, 0LL)
{
}

util::LongReference::LongReference(JNIEnv *env, jlong value) : m_env(env), m_object(NULL)
{
    jclass lClass;
    static const char *const className = "com/arifex/gsjava/util/LongReference";

    lClass = env->FindClass(className);
    if (lClass == NULL)
    {
        throwNoClassDefError(env, className);
        return;
    }

    jmethodID constructor = env->GetMethodID(lClass, "<init>", "(J)V");

    m_object = env->NewObject(lClass, constructor, value);
}

util::LongReference::LongReference(JNIEnv *env, jobject object) :
    m_env(env), m_object(object)
{
}

util::LongReference::~LongReference()
{
}

util::IntReference::IntReference(JNIEnv *env) : IntReference(env, (jint)0)
{
}

util::IntReference::IntReference(JNIEnv *env, jint value) : m_env(env), m_object(NULL)
{
    jclass iClass;
    static const char *const className = "com/artifex/gsjava/util/IntReference";

    iClass = env->FindClass(className);
    if (iClass == NULL)
    {
        throwNoClassDefError(env, className);
        return;
    }

    jmethodID constructor = env->GetMethodID(iClass, "<init>", "(I)V");

    m_object = env->NewObject(iClass, constructor, value);
}

util::IntReference::IntReference(JNIEnv *env, jobject object) :
    m_env(env), m_object(object)
{
}

util::IntReference::~IntReference()
{
}

jbyteArray util::ByteArrayReference::newByteArray(JNIEnv *env, const jbyte *data, jsize len)
{
    jbyteArray array = env->NewByteArray(len);
    env->SetByteArrayRegion(array, 0, len, data);
    return array;
}

util::ByteArrayReference::ByteArrayReference(JNIEnv *env) :
    ByteArrayReference(env, (jbyteArray)NULL)
{
}

util::ByteArrayReference::ByteArrayReference(JNIEnv *env, jbyteArray array) :
    m_env(env), m_object(NULL)
{
    jclass iClass;
    static const char *const className = "com/artifex/gsjava/util/ByteArrayReference";

    iClass = env->FindClass(className);
    if (iClass == NULL)
    {
        throwNoClassDefError(env, className);
        return;
    }

    jmethodID constructor = env->GetMethodID(iClass, "<init>", "([B)V");

    m_object = env->NewObject(iClass, constructor, array);
}

util::ByteArrayReference::ByteArrayReference(JNIEnv *env, const char *array, jsize len) :
    ByteArrayReference(env, (const signed char *)array, len)
{
}

util::ByteArrayReference::ByteArrayReference(JNIEnv *env, const signed char *array, jsize len) :
    ByteArrayReference(env, newByteArray(env, array, len))
{
}

util::ByteArrayReference::ByteArrayReference(JNIEnv *env, const unsigned char *array, jsize len) :
    ByteArrayReference(env, (const char *)array, len)
{
}

util::ByteArrayReference::ByteArrayReference(JNIEnv *env, jobject object) :
    m_env(env), m_object(object)
{
}

util::ByteArrayReference::~ByteArrayReference()
{
}
