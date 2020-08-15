package com.artifex.gsjava.util;

/**
 * Stores a reference to a byte array.
 *
 * @author Ethan Vrhel
 *
 */
public class ByteArrayReference extends Reference<byte[]> {

	public ByteArrayReference() {
		super();
	}

	public ByteArrayReference(final byte[] value) {
		super(value);
	}

	public String asString() {
		byte[] val = getValue();
		return val == null ? "null" : new String(val);
	}
}
