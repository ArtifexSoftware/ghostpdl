package com.artifex.gsjava.util;

import java.util.Arrays;

/**
 * Stores a reference to a byte array.
 *
 * @author Ethan Vrhel
 *
 */
public class ByteArrayReference {

	public volatile byte[] value;

	public ByteArrayReference() {
		this(null);
	}

	public ByteArrayReference(final byte[] value) {
		this.value = value;
	}

	@Override
	public String toString() {
		return Arrays.toString(value);
	}
}
