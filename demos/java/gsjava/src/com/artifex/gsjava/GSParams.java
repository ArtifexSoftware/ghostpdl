package com.artifex.gsjava;

import static com.artifex.gsjava.GSAPI.*;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.artifex.gsjava.util.ByteArrayReference;
import com.artifex.gsjava.util.Reference;

/**
 * The Params class allows for easier iteration over Ghostscript parameters.
 *
 * @author Ethan Vrhel
 *
 */
public class GSParams implements Iterable<GSParam<?>> {

	/**
	 * Gets an instance of the Params object for the given instance.
	 *
	 * @param instance The Ghostscript instance.
	 * @return A Params object for the given instance.
	 */
	public static GSParams getParams(long instance) {
		return new GSParams(instance);
	}

	private long instance;

	private GSParams(long instance) {
		this.instance = instance;
	}

	/**
	 * Converts this object into an array of <code>GSParam<?></code>s in the order
	 * returned by this object's iterator.
	 *
	 * @return An array with the same contents as the elements returned by this
	 * object's iterator.
	 */
	public GSParam<?>[] toArray() {
		List<GSParam<?>> params = new LinkedList<>();
		for (GSParam<?> param : this) {
			params.add(param);
		}
		return params.toArray(new GSParam<?>[params.size()]);
	}

	@Override
	public Iterator<GSParam<?>> iterator() {
		return new ParamIterator();
	}

	private class ParamIterator implements Iterator<GSParam<?>> {

		private Reference<Long> iterator;
		private ByteArrayReference key;
		private Reference<Integer> valueType;
		private Reference<?> value;

		private int returnCode;

		private ParamIterator() {
			iterator = new Reference<>(0L);
			key = new ByteArrayReference();
			valueType = new Reference<>();
			value = new Reference<>();
		}

		@Override
		public boolean hasNext() {
			long lastValue = iterator.getValue();
			returnCode = gsapi_enumerate_params(instance, iterator, null, null);
			iterator.setValue(lastValue);
			return returnCode != 1;
		}

		@Override
		public GSParam<?> next() {
			if (returnCode != 0)
				throw new IllegalStateException("Reached end of iterator");
			returnCode = gsapi_enumerate_params(instance, iterator, key, valueType);
			if (returnCode < 0)
				throw new IllegalStateException("Failed to enumerate params");

			int code = gsapi_get_param_once(instance, key.getValue(), value, valueType.getValue());
			if (code != 0)
				throw new IllegalStateException("Failed to get param");
			return new GSParam<>(key.asString(), value.getValue());
		}

	}
}
