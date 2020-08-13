package com.artifex.gsjava.util;

/**
 * Stores a reference to an integer.
 *
 * @author Ethan Vrhel
 *
 */
public class IntReference extends Reference<Integer> {

	//public volatile int value;

	public IntReference() {
		this(0);
	}

	public IntReference(final int value) {
		//this.value = value;
		super(value);
	}

	/*@Override
	public String toString() {
		return new StringBuilder().append(value).toString();
	}*/
}
