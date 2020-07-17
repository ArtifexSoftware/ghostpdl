#include "jni_util.h"

#include <exception>
#include <string.h>
#include <varargs.h>
#include <string>

using namespace util;

jfieldID util::getFieldID(JNIEnv *env, jobject object, const char *field, const char *sig)
{
    if (env == NULL || object == NULL || field == NULL || sig == NULL)
        return NULL;

    jclass clazz = env->GetObjectClass(object);
    jfieldID fieldID = env->GetFieldID(clazz, field, sig);
    if (fieldID == NULL)
    {
        const char *className = getClassName(env, clazz);

        const std::string full = std::string(className) + field;
        throwNoSuchFieldError(env, full.c_str());

        freeClassName(className);

        return NULL;
    }

    return fieldID;
}

jmethodID util::getMethodID(JNIEnv *env, jobject object, const char *method, const char *sig)
{
    if (env == NULL || object == NULL || method == NULL || sig == NULL)
        return NULL;

    jclass clazz = env->GetObjectClass(object);
    jmethodID methodID = env->GetMethodID(clazz, method, sig);
    if (methodID == NULL)
    {
        const char *className = getClassName(env, clazz);

        const std::string full = std::string(className) + method + sig;
        throwNoSuchMethodError(env, full.c_str());

        freeClassName(className);

        return NULL;
    }
    return methodID;
}


void util::setByteArrayField(JNIEnv *env, jobject object, const char *field, jbyteArray value)
{
    jfieldID fieldID = getFieldID(env, object, field, "[B");
    if (fieldID == NULL)
        return;

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
    jfieldID fieldID = getFieldID(env, object, field, "[B");
    if (fieldID == NULL)
        return NULL;

    return (jbyteArray)env->GetObjectField(object, fieldID);
}

char **util::jbyteArray2DToCharArray(JNIEnv *env, jobjectArray array)
{
    jboolean copy = false;
    jsize len = env->GetArrayLength(array);
    char **result = new char*[len];
    for (jsize i = 0; i < len; i++)
    {
        jbyteArray byteArrayObject = (jbyteArray)env->GetObjectArrayElement(array, i);
        char *elem = (char *)env->GetByteArrayElements(byteArrayObject, &copy);
        jsize slen = strlen(elem);
        char *nstring = new char[slen + 1LL];
        nstring[slen] = 0;
        memcpy(nstring, elem, slen);
        result[i] = nstring;
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
    jfieldID fieldID = getFieldID(env, object, field, "J");
    if (fieldID == NULL)
        return;

    env->SetLongField(object, fieldID, value);
}

jlong util::getLongField(JNIEnv *env, jobject object, const char *field)
{
    jfieldID fieldID = getFieldID(env, object, field, "J");
    if (fieldID == NULL)
        return 0LL;

    return env->GetLongField(object, fieldID);
}

void util::setIntField(JNIEnv *env, jobject object, const char *field, jint value)
{
    jfieldID fieldID = getFieldID(env, object, field, "I");
    if (fieldID == NULL)
        return;

    env->SetIntField(object, fieldID, value);
}

jint util::getIntField(JNIEnv *env, jobject object, const char *field)
{
    jfieldID fieldID = getFieldID(env, object, field, "I");
    if (fieldID == NULL)
        return 0;

    return env->GetIntField(object, fieldID);
}

int util::callIntMethod(JNIEnv *env, jobject object, const char *method, const char *sig, ...)
{
    jmethodID methodID = getMethodID(env, object, method, sig);
    if (methodID == NULL)
        return 0;

    va_list args;
    int result;
    va_start(args, sig);
    result = env->CallIntMethodV(object, methodID, args);
    va_end(args);
    return result;
}

jint util::throwNoClassDefError(JNIEnv *env, const char *message)
{
    if (env == NULL)
        return -1;

    jclass exClass;
    static const char *const className = "java/lang/NoClassDefFoundError";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        throw std::exception("Failed to find java.lang.NoClassDefFoundError");

    return env->ThrowNew(exClass, message);
}

jint util::throwNullPointerException(JNIEnv *env, const char *message)
{
    if (env == NULL)
        return -1;

    jclass exClass;
    static const char *const className = "java/lang/NullPointerException";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        return throwNoClassDefError(env, className);

    return env->ThrowNew(exClass, message);
}

jint util::throwNoSuchMethodError(JNIEnv *env, const char *message)
{
    if (env == NULL)
        return -1;

    jclass exClass;
    static const char *const className = "java/lang/NoSuchMethodError";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        return throwNoClassDefError(env, className);

    return env->ThrowNew(exClass, message);
}

jint util::throwNoSuchFieldError(JNIEnv *env, const char *message)
{
    if (env == NULL)
        return -1;

    jclass exClass;
    static const char *const className = "java/lang/NoSuchFieldError";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        return throwNoClassDefError(env, className);

    return env->ThrowNew(exClass, message);
}

const char *util::getClassName(JNIEnv *env, jclass clazz)
{
    jmethodID id = getMethodID(env, clazz, "getName", "()Ljava/lang/String;");
    if (id == NULL)
        return NULL;

    jobject name = env->CallObjectMethod(clazz, id);
    jclass sClass = env->GetObjectClass(name);
    jmethodID id = getMethodID(env, clazz, "getBytes", "()[B");
    if (id == NULL)
        return NULL;

    jbyteArray bname = (jbyteArray)env->CallObjectMethod(name, id);
    jsize len = env->GetArrayLength(bname);

    char *cstr = new char[len];
    if (cstr == NULL)
        return NULL;

    jboolean copy = false;
    jbyte *bytes = env->GetByteArrayElements(bname, &copy);

    memcpy(cstr, bytes, len);
    return cstr;
}

void util::freeClassName(const char *className)
{
    delete[] className;
}

util::LongReference::LongReference(JNIEnv *env) : LongReference(env, 0LL)
{
}

util::LongReference::LongReference(JNIEnv *env, jlong value) : m_env(env), m_object(NULL)
{
    if (env == NULL)
        return;

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

util::LongReference::~LongReference()
{
}

util::IntReference::IntReference(JNIEnv *env) : IntReference(env, (jint)0)
{
}

util::IntReference::IntReference(JNIEnv *env, jint value) : m_env(env), m_object(NULL)
{
    if (env == NULL)
        return;

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

util::IntReference::~IntReference()
{
}

jbyteArray util::ByteArrayReference::newByteArray(JNIEnv *env, const jbyte *data, jsize len)
{
    if (env == NULL)
        return NULL;

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
    if (env == NULL)
        return;

    jclass iClass;
    static const char *const className = "com/artifex/gsjava/util/ByteArrayReference";

    iClass = env->FindClass(className);
    if (iClass == NULL)
    {
        throwNoClassDefError(env, className);
        return;
    }

    jmethodID constructor = env->GetMethodID(iClass, "<init>", "([B)V");
    if (constructor == NULL)
    {
        throwNoSuchMethodError(env, "com.artifex.gsjava.util.ByteArrayReference.<init>([B)V");
        return;
    }

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

util::ByteArrayReference::~ByteArrayReference()
{
}