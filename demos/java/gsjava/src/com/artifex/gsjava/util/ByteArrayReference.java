package com.artifex.gsjava.util;

import java.util.Arrays;

/**
 * Stores a reference to a byte array.
 *
 * @author Ethan Vrhel
 *
 */
public class ByteArrayReference extends Reference<byte[]> {

	public ByteArrayReference() {
		this(null);
	}

	public ByteArrayReference(final byte[] value) {
		//this.value = value;
		super(value);
	}

	public String asString() {
		byte[] val = getValue();
		return val == null ? "null" : new String(val);
	}
}
