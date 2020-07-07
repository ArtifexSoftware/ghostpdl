package com.artifex.gsjava;

import java.util.Arrays;

public class ByteArrayReference {

	public volatile byte[] value;

	public ByteArrayReference() {
		this(null);
	}

	public ByteArrayReference(byte[] value) {
		this.value = value;
	}

	@Override
	public String toString() {
		return Arrays.toString(value);
	}
}
