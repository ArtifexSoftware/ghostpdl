#pragma once

#include <jni.h>

#include "settings.h"

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
	jint callIntMethod(JNIEnv *env, jobject object, const char *name, const char *sig, ...);

	void setObjectField(JNIEnv *env, jobject object, const char *field, jobject value);

	jobject getObjectField(JNIEnv *env, jobject object, const char *field);

	jobject toWrapperType(JNIEnv *env, jboolean value);
	jobject toWrapperType(JNIEnv *env, jbyte value);
	jobject toWrapperType(JNIEnv *env, jchar value);
	jobject toWrapperType(JNIEnv *env, jshort value);
	jobject toWrapperType(JNIEnv *env, jint value);
	jobject toWrapperType(JNIEnv *env, jlong value);
	jobject toWrapperType(JNIEnv *env, jfloat value);
	jobject toWrapperType(JNIEnv *env, jdouble value);

	jboolean toBoolean(JNIEnv *env, jobject wrapped);
	jbyte toByte(JNIEnv *env, jobject wrapped);
	jchar toChar(JNIEnv *env, jobject wrapped);
	jshort toShort(JNIEnv *env, jobject wrapped);
	jint toInt(JNIEnv *env, jobject wrapped);
	jlong toLong(JNIEnv *env, jobject wrapped);
	jfloat toFloat(JNIEnv *env, jobject wrapped);
	jdouble toDouble(JNIEnv *env, jobject wrapped);

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
	public:

		static inline void setValueField(JNIEnv *env, jobject object, jobject value)
		{
			setObjectField(env, object, "value", value);
		}

		static inline jobject getValueField(JNIEnv *env, jobject object)
		{
			return getObjectField(env, object, "value");
		}

	private:
		JNIEnv *m_env;
		jobject m_object;
	public:
		/*!
		Creates a new reference.

		@param env A JNIEnv.
		*/
		Reference(JNIEnv *env);

		/*!
		Creates a new reference.

		@param env A JNIEnv.
		@param object A com.artifex.gsjava.util.Reference or NULL if one
		should be created.
		*/
		Reference(JNIEnv *env, jobject object);
		~Reference();

		inline jobject object()
		{
			return m_object;
		}

		inline jobject value()
		{
			return getValueField(m_env, m_object);
		}

		inline jboolean booleanValue()
		{
			jobject val = value();
			return val ? toBoolean(m_env, val) : JNI_FALSE;
		}

		inline jbyte byteValue()
		{
			jobject val = value();
			return val ? toByte(m_env, val) : 0;
		}

		inline jchar charValue()
		{
			jobject val = value();
			return val ? toChar(m_env, val) : 0;
		}

		inline jshort shortValue()
		{
			jobject val = value();
			return val ? toShort(m_env, val) : 0;
		}

		inline jint intValue()
		{
			jobject val = value();
			return val ? toInt(m_env, val) : 0;
		}

		inline jlong longValue()
		{
			jobject val = value();
			return val ? toLong(m_env, val) : 0LL;
		}

		inline jfloat floatValue()
		{
			jobject val = value();
			return val ? toFloat(m_env, val) : 0.0f;
		}

		inline jdouble doubleValue()
		{
			jobject val = value();
			return val ? toDouble(m_env, val) : 0.0;
		}

		inline void set(jobject value)
		{
			setValueField(m_env, m_object, value);
		}

		inline void set(jboolean value)
		{
			set(toWrapperType(m_env, value));
		}

		inline void set(jbyte value)
		{
			set(toWrapperType(m_env, value));
		}

		inline void set(jshort value)
		{
			set(toWrapperType(m_env, value));
		}

		inline void set(jint value)
		{
			set(toWrapperType(m_env, value));
		}

		inline void set(jlong value)
		{
			set(toWrapperType(m_env, value));
		}

		inline void set(jfloat value)
		{
			set(toWrapperType(m_env, value));
		}

		inline void set(jdouble value)
		{
			set(toWrapperType(m_env, value));
		}
	};

}
