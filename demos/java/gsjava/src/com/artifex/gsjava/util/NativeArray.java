package com.artifex.gsjava.util;

/**
 * A NativeArray is an natively allocated array with a set length.
 *
 * @author Ethan Vrhel
 *
 * @param <T> The type stored in the array.
 */
public interface NativeArray<T> {

	/**
	 * Allocates the array of length <code>length</code>.
	 *
	 * @param length The length of the array.
	 * @throws IllegalStateException When the memory has already been allocated.
	 * @throws AllocationError When the allocation fails.
	 */
	public void allocate(long length) throws IllegalStateException, AllocationError;

	/**
	 * Sets the value of an element in the array.
	 *
	 * @param index The index to set.
	 * @param value The value to set to.
	 * @throws IndexOutOfBoundsException When <code>index</code> is less than 0 or
	 * greater or equal to the length.
	 * @throws NullPointerException When the array has not been allocated and is null.
	 */
	public void set(long index, T value) throws IndexOutOfBoundsException, NullPointerException;

	/**
	 * Returns the value at an index.
	 *
	 * @param index The index to get the value from.
	 * @return The respective value.
	 * @throws IndexOutOfBoundsException When <code>index</code> is less than 0 or
	 * greater or equal to the length.
	 * @throws NullPointerException When the array has not been allocated and is null.
	 */
	public T at(long index) throws IndexOutOfBoundsException, NullPointerException;

	/**
	 * Converts the native array to a Java array.
	 *
	 * @return A copy of the native array.
	 * @throws NullPointerException When the array has not been allocated and is null.
	 */
	public T[] toArray() throws NullPointerException;

	/**
	 * Converts the native array to a Java array without creating a new array ensuring
	 * that an array of <code>T</code>'s are returned. This will generally be faster
	 * than calling <code>toArray()</code>.
	 *
	 * @return A copy of the native array.
	 * @throws NullPointerException
	 */
	public Object toArrayNoConvert() throws NullPointerException;

	/**
	 * Returns the length of the array.
	 *
	 * @return The length.
	 */
	public long length();
}
