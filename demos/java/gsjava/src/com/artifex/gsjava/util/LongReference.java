package com.artifex.gsjava.util;

/**
 * Stores a reference to a long.
 *
 * @author Ethan Vrhel
 *
 */
public class LongReference {

	public volatile long value;

	public LongReference() {
		this(0L);
	}

	public LongReference(final long value) {
		this.value = value;
	}

	@Override
	public String toString() {
		return new StringBuilder().append(value).toString();
	}
}
