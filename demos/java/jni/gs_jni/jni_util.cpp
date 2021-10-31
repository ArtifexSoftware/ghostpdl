#include "jni_util.h"

#include <exception>
#include <string.h>
#include <stdarg.h>
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

        const std::string full = std::string(className) + "." + field;
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

        const std::string full = std::string(className) + "." + method + sig;
        throwNoSuchMethodError(env, full.c_str());

        freeClassName(className);

        env->DeleteLocalRef(clazz);
        return NULL;
    }

    env->DeleteLocalRef(clazz);
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
        jsize slen = (jsize)strlen(elem);
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

jint util::callIntMethod(JNIEnv *env, jobject object, const char *method, const char *sig, ...)
{
    jmethodID methodID = getMethodID(env, object, method, sig);
    if (methodID == NULL)
        return 0;

    va_list args;
    jint result;
    va_start(args, sig);
    result = env->CallIntMethodV(object, methodID, args);
    va_end(args);

    return result;
}

void util::setObjectField(JNIEnv *env, jobject object, const char *field, jobject value)
{
    jfieldID fieldID = getFieldID(env, object, field, "Ljava/lang/Object;");
    if (fieldID == NULL)
        return;

    env->SetObjectField(object, fieldID, value);
}

jobject util::getObjectField(JNIEnv *env, jobject object, const char *field)
{

    jfieldID fieldID = getFieldID(env, object, field, "Ljava/lang/Object;");
    if (fieldID == NULL)
        return 0;

    return env->GetObjectField(object, fieldID);
}

jobject util::toWrapperType(JNIEnv *env, jboolean value)
{
    jclass clazz;
    static const char *const className = "java/lang/Boolean";
    clazz = env->FindClass(className);
    if (!clazz)
    {
        throwNoClassDefError(env, className);
        return NULL;
    }
    jmethodID methodID = env->GetMethodID(clazz, "<init>", "(Z)V");
    jobject result = env->NewObject(clazz, methodID, value);
    return result;
}

jobject util::toWrapperType(JNIEnv *env, jbyte value)
{
    jclass clazz;
    static const char *const className = "java/lang/Byte";
    clazz = env->FindClass(className);
    if (!clazz)
    {
        throwNoClassDefError(env, className);
        return NULL;
    }
    jmethodID methodID = env->GetMethodID(clazz, "<init>", "(Z)V");
    jobject result = env->NewObject(clazz, methodID, value);
    return result;
}

jobject util::toWrapperType(JNIEnv *env, jchar value)
{
    jclass clazz;
    static const char *const className = "java/lang/Character";
    clazz = env->FindClass(className);
    if (!clazz)
    {
        throwNoClassDefError(env, className);
        return NULL;
    }
    jmethodID methodID = env->GetMethodID(clazz, "<init>", "(C)V");
    jobject result = env->NewObject(clazz, methodID, value);
    return result;
}

jobject util::toWrapperType(JNIEnv *env, jshort value)
{
    jclass clazz;
    static const char *const className = "java/lang/Short";
    clazz = env->FindClass(className);
    if (!clazz)
    {
        throwNoClassDefError(env, className);
        return NULL;
    }
    jmethodID methodID = env->GetMethodID(clazz, "<init>", "(S)V");
    jobject result = env->NewObject(clazz, methodID, value);
    return result;
}

jobject util::toWrapperType(JNIEnv *env, jint value)
{
    jclass clazz;
    static const char *const className = "java/lang/Integer";
    clazz = env->FindClass(className);
    if (!clazz)
    {
        throwNoClassDefError(env, className);
        return NULL;
    }
    jmethodID methodID = env->GetMethodID(clazz, "<init>", "(I)V");
    jobject result = env->NewObject(clazz, methodID, value);
    return result;
}

jobject util::toWrapperType(JNIEnv *env, jlong value)
{
    jclass clazz;
    static const char *const className = "java/lang/Long";
    clazz = env->FindClass(className);
    if (!clazz)
    {
        throwNoClassDefError(env, className);
        return NULL;
    }
    jmethodID methodID = env->GetMethodID(clazz, "<init>", "(J)V");
    jobject result = env->NewObject(clazz, methodID, value);
    return result;
}

jobject util::toWrapperType(JNIEnv *env, jfloat value)
{
    jclass clazz;
    static const char *const className = "java/lang/Float";
    clazz = env->FindClass(className);
    if (!clazz)
    {
        throwNoClassDefError(env, className);
        return NULL;
    }
    jmethodID methodID = env->GetMethodID(clazz, "<init>", "(F)V");
    jobject result = env->NewObject(clazz, methodID, value);
    return result;
}

jobject util::toWrapperType(JNIEnv *env, jdouble value)
{
    jclass clazz;
    static const char *const className = "java/lang/Double";
    clazz = env->FindClass(className);
    if (!clazz)
    {
        throwNoClassDefError(env, className);
        return NULL;
    }
    jmethodID methodID = env->GetMethodID(clazz, "<init>", "(D)V");
    jobject result = env->NewObject(clazz, methodID, value);
    return result;
}

jboolean util::toBoolean(JNIEnv *env, jobject wrapped)
{
    if (!wrapped)
    {
        throwNullPointerException(env, "attempting to get wrapped jboolean on NULL jobject");
        return JNI_FALSE;
    }
    jclass clazz = env->GetObjectClass(wrapped);
    jfieldID fieldID = env->GetFieldID(clazz, "value", "Z");
    if (fieldID == NULL)
    {
        throwNoSuchFieldError(env, "value");
        return 0;
    }
    return env->GetBooleanField(wrapped, fieldID);
}

jbyte util::toByte(JNIEnv *env, jobject wrapped)
{
    if (!wrapped)
    {
        throwNullPointerException(env, "attempting to get wrapped jbyte on NULL jobject");
        return 0;
    }

    jclass clazz = env->GetObjectClass(wrapped);
    jfieldID fieldID = env->GetFieldID(clazz, "value", "B");
    if (fieldID == NULL)
    {
        throwNoSuchFieldError(env, "value");
        return 0;
    }
    return env->GetByteField(wrapped, fieldID);
}

jchar util::toChar(JNIEnv *env, jobject wrapped)
{
    if (!wrapped)
    {
        throwNullPointerException(env, "attempting to get wrapped jchar on NULL jobject");
        return 0;
    }

    jclass clazz = env->GetObjectClass(wrapped);
    jfieldID fieldID = env->GetFieldID(clazz, "value", "C");
    if (fieldID == NULL)
    {
        throwNoSuchFieldError(env, "value");
        return 0;
    }
    return env->GetCharField(wrapped, fieldID);
}

jshort util::toShort(JNIEnv *env, jobject wrapped)
{
    if (!wrapped)
    {
        throwNullPointerException(env, "attempting to get wrapped jshort on NULL jobject");
        return 0;
    }
    jclass clazz = env->GetObjectClass(wrapped);
    jfieldID fieldID = env->GetFieldID(clazz, "value", "S");
    if (fieldID == NULL)
    {
        throwNoSuchFieldError(env, "value");
        return 0;
    }
    return env->GetShortField(wrapped, fieldID);
}

jint util::toInt(JNIEnv *env, jobject wrapped)
{
    if (!wrapped)
    {
        throwNullPointerException(env, "attempting to get wrapped jint on NULL jobject");
        return 0;
    }

    jclass clazz = env->GetObjectClass(wrapped);
    jfieldID fieldID = env->GetFieldID(clazz, "value", "I");
    if (fieldID == NULL)
    {
        throwNoSuchFieldError(env, "value");
        return 0;
    }
    return env->GetIntField(wrapped, fieldID);
}

jlong util::toLong(JNIEnv *env, jobject wrapped)
{
    if (!wrapped)
    {
        throwNullPointerException(env, "attempting to get wrapped jlong on NULL jobject");
        return 0;
    }

    jclass clazz = env->GetObjectClass(wrapped);
    jfieldID fieldID = env->GetFieldID(clazz, "value", "J");
    if (fieldID == NULL)
    {
        throwNoSuchFieldError(env, "value");
        return 0;
    }
    return env->GetLongField(wrapped, fieldID);
}

jfloat util::toFloat(JNIEnv *env, jobject wrapped)
{
    if (!wrapped)
    {
        throwNullPointerException(env, "attempting to get wrapped jfloat on NULL jobject");
        return 0;
    }

    jclass clazz = env->GetObjectClass(wrapped);
    jfieldID fieldID = env->GetFieldID(clazz, "value", "F");
    if (fieldID == NULL)
    {
        throwNoSuchFieldError(env, "value");
        return 0;
    }
    return env->GetFloatField(wrapped, fieldID);
}

jdouble util::toDouble(JNIEnv *env, jobject wrapped)
{
    if (!wrapped)
    {
        throwNullPointerException(env, "attempting to get wrapped jdouble on NULL jobject");
        return 0;
    }

    jclass clazz = env->GetObjectClass(wrapped);
    jfieldID fieldID = env->GetFieldID(clazz, "value", "D");
    if (fieldID == NULL)
    {
        throwNoSuchFieldError(env, "value");
        return 0;
    }
    return env->GetDoubleField(wrapped, fieldID);
}

jint util::throwNoClassDefError(JNIEnv *env, const char *message)
{
    if (env == NULL)
        return -1;

    jclass exClass;
    static const char *const className = "java/lang/NoClassDefFoundError";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        return -2;
        //throw std::exception("Failed to find java.lang.NoClassDefFoundError");

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

jint util::throwAllocationError(JNIEnv *env, const char *message)
{
    if (env == NULL)
        return -1;

    jclass exClass;
    static const char *const className = "com/artifex/gsjava/util/AllocationError";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        return throwNoClassDefError(env, className);

    return env->ThrowNew(exClass, message);
}

jint util::throwIllegalArgumentException(JNIEnv *env, const char *message)
{
    if (env == NULL)
        return -1;

    jclass exClass;
    static const char *const className = "java/lang/IllegalArgumentException";

    exClass = env->FindClass(className);
    if (exClass == NULL)
        return throwNoClassDefError(env, className);

    return env->ThrowNew(exClass, message);
}

const char *util::getClassName(JNIEnv *env, jclass clazz)
{
    jclass clsClazz = env->GetObjectClass(clazz);

    jmethodID methodId = env->GetMethodID(clsClazz, "getName", "()Ljava/lang/String;");
    jstring className = (jstring)env->CallObjectMethod(clazz, methodId);
    const char *chars = env->GetStringUTFChars(className, NULL);
    jsize len = env->GetStringLength(className);
    char *cstr = new char[len + 1LL];
    if (cstr == NULL)
    {
        env->ReleaseStringUTFChars(className, chars);
        env->DeleteLocalRef(className);
        return NULL;
    }
    cstr[len] = 0;
    memcpy(cstr, chars, len);
    env->ReleaseStringUTFChars(className, chars);
    env->DeleteLocalRef(className);
    return cstr;
}

void util::freeClassName(const char *className)
{
    delete[] className;
}

util::Reference::Reference(JNIEnv *env) : Reference(env, NULL)
{
}

util::Reference::Reference(JNIEnv *env, jobject object) : m_env(env), m_object(NULL)
{
    if (!object)
    {
        static const char *const CLASS_NAME = "com/artifex/gsjava/util/Reference";
        jclass refClass = env->FindClass(CLASS_NAME);
        if (refClass == NULL)
        {
            throwNoClassDefError(env, CLASS_NAME);
            return;
        }
        jmethodID constructor = env->GetMethodID(refClass, "<init>", "()V");
        if (constructor == NULL)
        {
            throwNoSuchMethodError(env, "com/artifex/gsjava/util/Reference.<init>()V");
            return;
        }
        object = m_env->NewObject(refClass, constructor);
        if (object == NULL)
            return;
    }
    m_object = m_env->NewGlobalRef(object);
}

util::Reference::~Reference()
{
    if (m_object)
        m_env->DeleteGlobalRef(m_object);
}
