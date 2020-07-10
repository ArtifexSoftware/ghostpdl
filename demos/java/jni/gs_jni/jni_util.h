#pragma once

#include <jni.h>

namespace util
{
	typedef class LongReference LongReference;
	typedef class IntReference IntReference;
	typedef class ByteArrayReference ByteArrayReference;

	class LongReference
	{
	private:
		JNIEnv *m_env;
		jobject m_object;
	public:
		LongReference(JNIEnv *env);
		LongReference(JNIEnv *env, jlong value);
		LongReference(JNIEnv *env, jobject object);
		~LongReference();

		inline void setValue(jlong value)
		{
			setLongField(m_env, m_object, "value", value);
		}

		inline jlong value() const
		{
			return getLongField(m_env, m_object, "value");
		}

		inline jobject object()
		{
			return m_object;
		}

		inline const jobject object() const
		{
			return m_object;
		}

		inline LongReference &asGlobal()
		{
			m_object = m_env->NewGlobalRef(m_object);
			return *this;
		}

		inline LongReference &deleteGlobal()
		{
			m_env->DeleteGlobalRef(m_object);
			return *this;
		}
	};

	class IntReference
	{
	private:
		JNIEnv *m_env;
		jobject m_object;
	public:
		IntReference(JNIEnv *env);
		IntReference(JNIEnv *env, jint value);
		IntReference(JNIEnv *env, jobject object);
		~IntReference();

		inline void setValue(jint value)
		{
			setIntField(m_env, m_object, "value", value);
		}

		inline jint value() const
		{
			return getIntField(m_env, m_object, "value");
		}

		inline jobject object()
		{
			return m_object;
		}

		inline const jobject object() const
		{
			return m_object;
		}

		inline IntReference &asGlobal()
		{
			m_object = m_env->NewGlobalRef(m_object);
			return *this;
		}

		inline IntReference &deleteGlobal()
		{
			m_env->DeleteGlobalRef(m_object);
			return *this;
		}
	};

	class ByteArrayReference
	{
	private:
		static jbyteArray newByteArray(JNIEnv *env, const jbyte *data, jsize len);

		JNIEnv *m_env;
		jobject m_object;
	public:
		ByteArrayReference(JNIEnv *env);
		ByteArrayReference(JNIEnv *env, jbyteArray array);
		ByteArrayReference(JNIEnv *env, const char *array, jsize len);
		ByteArrayReference(JNIEnv *env, const signed char *array, jsize len);
		ByteArrayReference(JNIEnv *env, const unsigned char *array, jsize len);
		ByteArrayReference(JNIEnv *env, jobject object);
		~ByteArrayReference();

		inline void setValue(jbyteArray value)
		{
			setByteArrayField(m_env, m_object, "value", value);
		}

		inline void setValue(const char *array)
		{
			setByteArrayField(m_env, m_object, "value", array);
		}

		inline void setValue(const signed char *array)
		{
			setValue((const char *)array);
		}

		inline void setValue(const unsigned char *array)
		{
			setValue((const char *)array);
		}

		inline jbyteArray value() const
		{
			return getByteArrayField(m_env, m_object, "value");
		}

		inline ByteArrayReference &asGlobal()
		{
			m_object = m_env->NewGlobalRef(m_object);
			return *this;
		}

		inline ByteArrayReference &deleteGlobal()
		{
			m_env->DeleteGlobalRef(m_object);
			return *this;
		}
	};

	void setByteArrayField(JNIEnv *env, jobject object, const char *field, jbyteArray value);
	void setByteArrayField(JNIEnv *env, jobject object, const char *field, const char *string);
	jbyteArray getByteArrayField(JNIEnv *env, jobject object, const char *field);

	char **jbyteArray2DToCharArray(JNIEnv *env, jobjectArray array);
	void delete2DByteArray(int count, char **array);

	void setLongField(JNIEnv *env, jobject object, const char *field, jlong value);
	jlong getLongField(JNIEnv *env, jobject object, const char *field);

	void setIntField(JNIEnv *env, jobject object, const char *field, jint value);
	jint getIntField(JNIEnv *env, jobject object, const char *field);

	int callIntMethod(JNIEnv *env, jobject object, const char *name, const char* sig, ...);

	jint throwNoClassDefError(JNIEnv *env, const char *message);
	jint throwNullPointerException(JNIEnv *env, const char *message);
}