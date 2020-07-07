package com.artifex.gsjava;

public class IntReference {

	public volatile int value;

	public IntReference() {
		this(0);
	}

	public IntReference(int value) {
		this.value = value;
	}

	@Override
	public String toString() {
		return new StringBuilder().append(value).toString();
	}
}
