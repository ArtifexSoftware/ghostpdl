#pragma once

#include <jni.h>

#define REFERENCE_VALUE_FILED_NAME "value"

namespace util
{

	typedef class Reference Reference;
	typedef class LongReference LongReference;
	typedef class IntReference IntReference;
	typedef class ByteArrayReference ByteArrayReference;

	/*!
	Returns the field ID of a field inside an object, checking if it exists and ensuring
	all arguments are non-null. If the field is not found, the Java exception NoSuchFieldError
	will be thrown and NULL will be returned.

	@param env A JNIEnv.
	@param object The object to find the field in.
	@param field The name of the field.
	@param sig The field's signature.

	@return The field ID, or NULL if the field is not found, or any argument is NULL.
	*/
	jfieldID getFieldID(JNIEnv *env, jobject object, const char *field, const char *sig);

	/*!
	Returns the method ID of a method inside an object, checking if it exists and ensuring
	all arguments are non-null. If the method is not found, the Java exception NoSuchMethodError
	will be thrown and NULL will be returned.

	@param env A JNIEnv.
	@param object The object to find the method in.
	@param method The name of the method.
	@param sig The method's signature.

	@return The method ID, or NULL if the method is not found, or any argument is NULL.
	*/
	jmethodID getMethodID(JNIEnv *env, jobject object, const char *method, const char *sig);

	/*!
	Sets a byte array field of an object.

	@param env A JNIEnv.
	@param object The object containing a byte array field.
	@param field The name of the field to set.
	@param value The value to set the field to.
	*/
	void setByteArrayField(JNIEnv *env, jobject object, const char *field, jbyteArray value);

	/*!
	Sets a byte array field of an object.

	@param env A JNIEnv.
	@param object The object containing a byte array field.
	@param field The name of the field to set.
	@param string The value to set the field to.
	*/
	void setByteArrayField(JNIEnv *env, jobject object, const char *field, const char *string);

	/*!
	Returns the value of a byte array field in an object.

	@param env A JNIEnv.
	@param object The object to get the byte array field from.
	@param field The name of the field.

	@return The value of the field.
	*/
	jbyteArray getByteArrayField(JNIEnv *env, jobject object, const char *field);

	/*!
	Converts a 2D Java byte array to a 2D C++ char array. The returned
	value should be used in delete2DByteArray() when finished.

	@param env A JNIEnv.
	@param array The array to convert.

	@return The 2D C++ char array.
	*/
	char **jbyteArray2DToCharArray(JNIEnv *env, jobjectArray array);

	/*!
	Deletes a 2D C++ char array allocated from jbyteArray2DToCharArray().

	@param count The size of the array.
	@param array The array to delete.
	*/
	void delete2DByteArray(int count, char **array);

	/*!
	Sets a long field of an object.

	@param env A JNIEnv.
	@param object The object containing a long field.
	@param field The name of the field to set.
	@param value The value to set the field to.
	*/
	void setLongField(JNIEnv *env, jobject object, const char *field, jlong value);

	/*!
	Returns the value of a long field in an object.

	@param env A JNIEnv.
	@param object The object to get the long field from.
	@param field The name of the long field.

	@return The value of the long field.
	*/
	jlong getLongField(JNIEnv *env, jobject object, const char *field);

	/*!
	Sets a int field of an object.

	@param env A JNIEnv.
	@param object The object containing a int field.
	@param field The name of the field to set.
	@param value The value to set the field to.
	*/
	void setIntField(JNIEnv *env, jobject object, const char *field, jint value);

	/*!
	Returns the value of a int field in an object.

	@param env A JNIEnv.
	@param object The object to get the int field from.
	@param field The name of the int field.

	@return The value of the int field.
	*/
	jint getIntField(JNIEnv *env, jobject object, const char *field);

	/*!
	Calls a Java method returning an int.

	@param env A JNIEnv.
	@param object The object containing the int method.
	@param name The name of the method.
	@param sig The method's signature.
	@param ... The varargs representing the object's arguments in their respective order.
	*/
	int callIntMethod(JNIEnv *env, jobject object, const char *name, const char *sig, ...);

	void setObjectField(JNIEnv *env, jobject object, const char *field, jobject value);

	jobject getObjectField(JNIEnv *env, jobject object, const char *field);
	/*
	jobject toWrapperType(jboolean value);
	jobject toWrapperType(jchar value);
	jobject toWrapperType(jshort value);
	jobject toWrapperType(jint value);
	jobject toWrapperType(jlong value);
	jobject toWrapperType(jfloat value);
	jobject toWrapperType(jdouble value);*/

	/*!
	Throws the Java exception java.lang.NoClassDefFoundError with a message. The function
	calling this function should immediately return after calling this function.

	@param env A JNIEnv.
	@param message The message of the error.

	@return The result of throwing the error.
	*/
	jint throwNoClassDefError(JNIEnv *env, const char *message);

	/*!
	Throws the Java exception java.lang.NullPointerException with a message. The function
	calling this function should immediately return after calling this function.

	@param env A JNIEnv.
	@param message The message of the exception.

	@return The result of throwing the exception.
	*/
	jint throwNullPointerException(JNIEnv *env, const char *message);

	/*!
	Throws the Java exception java.lang.NoSuchMethodError with a message. The function
	calling this function should immediately return after calling this function.

	@param env A JNIEnv.
	@param message The message of the exception.

	@return The result of throwing the exception.
	*/
	jint throwNoSuchMethodError(JNIEnv *env, const char *message);

	/*!
	Throws the Java exception java.lang.NoSuchFieldError with a message. The function
	calling this function should immediately return after calling this function.

	@param env A JNIEnv.
	@param message The message of the exception.

	@return The result of throwing the exception.
	*/
	jint throwNoSuchFieldError(JNIEnv *env, const char *message);

	/*!
	Throws the Java exception com.artifex.gsjava.util.AllocationError with a message.
	The function calling this function should immediately return after calling this function.

	@param env A JNIEnv.
	@param message The message of the exception.

	@return The result of throwing the exception.
	*/
	jint throwAllocationError(JNIEnv *env, const char *message);

	jint throwIllegalArgumentException(JNIEnv *env, const char *message);

	/*!
	Returns the name of a jclass. The name is dynamically allocated and after usage,
	freeClassName() should be called.

	@param env A JNIEnv.
	@param clazz A jclass.

	@return The name of the class, or NULL if env or clazz are NULL.
	*/
	const char *getClassName(JNIEnv *env, jclass clazz);

	/*!
	Frees a class name generated from getClassName().

	@param className The className generated from getClassName().
	*/
	void freeClassName(const char *className);

	class Reference
	{
	private:
		JNIEnv *m_env;
		jobject m_object;
	public:

		static inline void setValueField(JNIEnv *env, jobject object, jobject value)
		{
			setObjectField(env, object, "value", value);
		}

		static inline jobject getValueField(JNIEnv *env, jobject object)
		{
			return getObjectField(env, object, "value");
		}
	};

	/*!
	Class representing the class com.artifex.gsjava.util.LongReference.
	*/
	class LongReference
	{
	private:
		JNIEnv *m_env;
		jobject m_object;
	public:

		/*!
		Sets the value field of a LongReference.

		@param env A JNIEnv.
		@param object A LongReference.
		@param value The value to set the field to.
		*/
		static inline void setValueField(JNIEnv *env, jobject object, jlong value)
		{
			setLongField(env, object, REFERENCE_VALUE_FILED_NAME, value);
		}

		/*!
		Returns the value of the field in a LongReference.

		@param env A JNIEnv.
		@param object A LongReference.

		@return The value of the LongReference's field.
		*/
		static inline jlong getValueField(JNIEnv *env, jobject object)
		{
			return getLongField(env, object, REFERENCE_VALUE_FILED_NAME);
		}

		LongReference(JNIEnv *env);
		LongReference(JNIEnv *env, jlong value);
		~LongReference();

		/*!
		Sets the value of this LongReference.

		@param value The value to set to.
		*/
		inline void setValue(jlong value)
		{
			setValueField(m_env, m_object, value);
		}

		/*!
		Returns the value contained in this LongReference.

		@return The value in this LongReference.
		*/
		inline jlong value() const
		{
			return getValueField(m_env, m_object);
		}

		/*!
		Returns the internal Java LongReference object.

		@param The Java LongReference.
		*/
		inline jobject object()
		{
			return m_object;
		}


		/*!
		Returns the internal Java LongReference object.

		@param The Java LongReference.
		*/
		inline const jobject object() const
		{
			return m_object;
		}

		/*!
		Marks this LongReference as a global reference, telling Java that this
		object should not be garbage collected.

		@return A reference to this LongReference.
		*/
		inline LongReference &asGlobal()
		{
			m_object = m_env->NewGlobalRef(m_object);
			return *this;
		}

		/*!
		Unmarks this LongReference as a global reference, telling Java that this
		object may be garbage collected.

		@return A reference to this LongReference.
		*/
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
		static inline void setValueField(JNIEnv *env, jobject object, jint value)
		{
			setIntField(env, object, REFERENCE_VALUE_FILED_NAME, value);
		}

		static inline jint getValueField(JNIEnv *env, jobject object)
		{
			return getIntField(env, object, REFERENCE_VALUE_FILED_NAME);
		}

		IntReference(JNIEnv *env);
		IntReference(JNIEnv *env, jint value);
		~IntReference();

		inline void setValue(jint value)
		{
			setValueField(m_env, m_object, value);
		}

		inline jint value() const
		{
			return getValueField(m_env, m_object);
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
		static inline void setValueField(JNIEnv *env, jobject object, jbyteArray value)
		{
			setByteArrayField(env, object, REFERENCE_VALUE_FILED_NAME, value);
		}

		static inline void setValueField(JNIEnv *env, jobject object, const char *array)
		{
			setByteArrayField(env, object, REFERENCE_VALUE_FILED_NAME, array);
		}

		static inline void setValueField(JNIEnv *env, jobject object, const signed char *array)
		{
			setValueField(env, object, (const char *)array);
		}

		static inline void setValueField(JNIEnv *env, jobject object, const unsigned char *array)
		{
			setValueField(env, object, (const char *)array);
		}

		static inline jbyteArray getValueField(JNIEnv *env, jobject object)
		{
			return getByteArrayField(env, object, "value");
		}

		ByteArrayReference(JNIEnv *env);
		ByteArrayReference(JNIEnv *env, jbyteArray array);
		ByteArrayReference(JNIEnv *env, const char *array, jsize len);
		ByteArrayReference(JNIEnv *env, const signed char *array, jsize len);
		ByteArrayReference(JNIEnv *env, const unsigned char *array, jsize len);
		~ByteArrayReference();

		inline void setValue(jbyteArray value)
		{
			setValueField(m_env, m_object, value);
		}

		inline void setValue(const char *array)
		{
			setValueField(m_env, m_object, array);
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
			return getValueField(m_env, m_object);
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
}
