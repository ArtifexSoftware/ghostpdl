package com.artifex.gsjava.util;

public class Reference<T> {

	private volatile T value;

	public Reference() {
		this(null);
	}

	public Reference(T value) {
		this.value = value;
	}

	public void setValue(T value) {
		this.value = value;
	}

	public T getValue() {
		return value;
	}

	@Override
	public boolean equals(Object o) {
		if (o == null)
			return false;
		if (o == this)
			return true;
		if (o instanceof Reference) {
			Reference<?> ref = (Reference<?>)o;
			return value == null ? ref.value == null : value.equals(ref.value);
		}
		return false;
	}

	@Override
	public int hashCode() {
		return value == null ? 0 : value.hashCode();
	}

	@Override
	public String toString() {
		return "Reference<" + (value == null ? "?" : value.getClass().getName()) + "> -> " + value;
	}
}
