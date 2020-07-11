package com.artifex.gsjava.util;

public class StringUtil {

	public static byte[] toNullTerminatedByteArray(final String str) {
		final byte[] barray = str.getBytes();
		final byte[] result = new byte[barray.length + 1];
		System.arraycopy(barray, 0, result, 0, barray.length);
		return result;
	}

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
