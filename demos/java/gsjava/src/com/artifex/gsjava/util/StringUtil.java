package com.artifex.gsjava.util;

/**
 * Contains utility methods to work with Strings.
 *
 * @author Ethan Vrhel
 *
 */
public class StringUtil {

	/**
	 * Converts a Java String to a null-terminated byte array.
	 *
	 * @param str The string to convert.
	 * @return The result byte array.
	 */
	public static byte[] toNullTerminatedByteArray(final String str) {
		final byte[] barray = str.getBytes();
		final byte[] result = new byte[barray.length + 1];
		System.arraycopy(barray, 0, result, 0, barray.length);
		return result;
	}

	/**
	 * Converts an array of Strings to a 2D byte array of null-terminated
	 * strings.
	 *
	 * @param strs The strings to convert.
	 * @return THe result 2D byte array.
	 */
	public static byte[][] to2DByteArray(final String[] strs){
		final byte[][] array = new byte[strs.length][];
		for (int i = 0; i < strs.length; i++) {
			final String str = strs[i];
			array[i] = toNullTerminatedByteArray(str);
		}
		return array;
	}

	private StringUtil() { }
}
