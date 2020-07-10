package com.artifex.gsjava.util;

public class StringUtil {

	public static byte[] toNullTerminatedByteArray(final String str) {
		final byte[] barray = str.getBytes();
		final byte[] result = new byte[barray.length + 1];
		System.arraycopy(barray, 0, result, 0, barray.length);
		return result;
	}

	private StringUtil() { }
}
