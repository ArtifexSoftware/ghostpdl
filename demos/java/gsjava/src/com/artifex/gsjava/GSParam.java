package com.artifex.gsjava;

public class GSParam<V> {

	public final String key;
	public final V value;

	public GSParam(String key, V value) {
		this.key = key;
		this.value = value;
	}

	public GSParam(V value) {
		this(null, value);
	}

	public boolean canConvertToString() {
		return value instanceof String || value instanceof byte[];
	}

	public String stringValue() {
		if (value instanceof String)
			return (String)value;
		else if (value instanceof byte[])
			return new String((byte[])value);
		throw new IllegalStateException("Value cannot be converted to string");
	}

	@Override
	public String toString() {
		return key + " -> " + (canConvertToString() ? stringValue() : value);
	}

	@Override
	public boolean equals(Object o) {
		if (o == null)
			return false;
		if (o == this)
			return true;
		if (o instanceof GSParam) {
			GSParam<?> other = (GSParam<?>)o;
			return (key == null ? other.key == null : key.equals(other.key)) &&
					(value == null ? other.value == null : value.equals(other.value));
		}
		return false;
	}

	@Override
	public int hashCode() {
		return key.hashCode() + (value == null ? 0 : value.hashCode());
	}
}
