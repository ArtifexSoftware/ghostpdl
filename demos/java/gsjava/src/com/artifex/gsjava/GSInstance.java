package com.artifex.gsjava;

import static com.artifex.gsjava.GSAPI.*;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.callbacks.ICalloutFunction;
import com.artifex.gsjava.callbacks.IPollFunction;
import com.artifex.gsjava.callbacks.IStdErrFunction;
import com.artifex.gsjava.callbacks.IStdInFunction;
import com.artifex.gsjava.callbacks.IStdOutFunction;
import com.artifex.gsjava.util.ByteArrayReference;
import com.artifex.gsjava.util.Reference;

/**
 * Utility class to make Ghostscript calls easier by storing a
 * Ghostscript instance and, optionally, a caller handle.
 *
 * @author Ethan Vrhel
 *
 */
public class GSInstance implements Iterable<GSInstance.GSParam<?>> {

	public static class GSParam<V> {

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

	private static boolean ALLOW_MULTITHREADING = false;
	private static volatile int INSTANCES = 0;

	public static void setAllowMultithreading(boolean state) {
		ALLOW_MULTITHREADING = state;
	}

	public static int getInstanceCount() {
		return INSTANCES;
	}

	private long instance;
	private long callerHandle;

	public GSInstance(long callerHandle) throws IllegalStateException {
		if (!ALLOW_MULTITHREADING && INSTANCES > 0)
			throw new IllegalStateException("An instance already exists");
		Reference<Long> ref = new Reference<>();
		int ret = gsapi_new_instance(ref, callerHandle);
		if (ret != GS_ERROR_OK)
			throw new IllegalStateException("Failed to create new instance: " + ret);
		this.instance = ref.getValue();
		this.callerHandle = callerHandle;
		INSTANCES++;
	}

	public GSInstance() throws IllegalStateException {
		this(GS_NULL);
	}

	public void delete_instance() {
		if (instance != GS_NULL) {
			gsapi_delete_instance(instance);
			instance = GS_NULL;
			INSTANCES--;
		}
	}

	public int set_stdio(IStdInFunction stdin, IStdOutFunction stdout, IStdErrFunction stderr) {
		return gsapi_set_stdio_with_handle(instance, stdin, stdout, stderr, callerHandle);
	}

	public int set_poll(IPollFunction pollfun) {
		return gsapi_set_poll_with_handle(instance, pollfun, callerHandle);
	}

	public int set_display_callback(DisplayCallback displaycallback) {
		return gsapi_set_display_callback(instance, displaycallback);
	}

	public int register_callout(ICalloutFunction callout) {
		return gsapi_register_callout(instance, callout, callerHandle);
	}

	public void deregister_callout(ICalloutFunction callout) {
		gsapi_deregister_callout(instance, callout, callerHandle);
	}

	public int set_arg_encoding(int encoding) {
		return gsapi_set_arg_encoding(instance, encoding);
	}

	public int set_default_device_list(byte[] list, int listlen) {
		return gsapi_set_default_device_list(instance, list, listlen);
	}

	public int get_default_device_list(Reference<byte[]> list, Reference<Integer> listlen) {
		return gsapi_get_default_device_list(instance, list, listlen);
	}

	public int init_with_args(int argc, byte[][] argv) {
		return gsapi_init_with_args(instance, argc, argv);
	}

	public int init_with_args(String[] argv) {
		return gsapi_init_with_args(instance, argv);
	}

	public int init_with_args(List<String> argv) {
		return gsapi_init_with_args(instance, argv);
	}

	public int run_string_begin(int userErrors, Reference<Integer> pExitCode) {
		return gsapi_run_string_begin(instance, userErrors, pExitCode);
	}

	public int run_string_continue(byte[] str, int length, int userErrors, Reference<Integer> pExitCode) {
		return gsapi_run_string_continue(instance, str, length, userErrors, pExitCode);
	}

	public int run_string_continue(String str, int length, int userErrors, Reference<Integer> pExitCode) {
		return gsapi_run_string_continue(instance, str, length, userErrors, pExitCode);
	}

	public int run_string(byte[] str, int userErrors, Reference<Integer> pExitCode) {
		return gsapi_run_string(instance, str, userErrors, pExitCode);
	}

	public int run_string(String str, int userErrors, Reference<Integer> pExitCode) {
		return gsapi_run_string(instance, str, userErrors, pExitCode);
	}

	public int run_file(byte[] fileName, int userErrors, Reference<Integer> pExitCode) {
		return gsapi_run_file(instance, fileName, userErrors, pExitCode);
	}

	public int run_file(String filename, int userErrors, Reference<Integer> pExitCode) {
		return gsapi_run_file(instance, filename, userErrors, pExitCode);
	}

	public int exit() {
		return gsapi_exit(instance);
	}

	public int set_param(byte[] param, Object value, int paramType) {
		return gsapi_set_param(instance, param, value, paramType);
	}

	public int set_param(String param, String value, int paramType) {
		return gsapi_set_param(instance, param, value, paramType);
	}

	public int set_param(String param, Object value, int paramType) {
		return gsapi_set_param(instance, param, value, paramType);
	}

	public int get_param(byte[] param, long value, int paramType) {
		return gsapi_get_param(instance, param, value, paramType);
	}

	public int get_param(String param, long value, int paramType) {
		return gsapi_get_param(instance, param, value, paramType);
	}

	public int get_param_once(byte[] param, Reference<?> value, int paramType) {
		return gsapi_get_param_once(instance, param, value, paramType);
	}

	public int get_param_once(String param, Reference<?> value, int paramType) {
		return gsapi_get_param_once(instance, param, value, paramType);
	}

	public int enumerate_params(Reference<Long> iter, Reference<byte[]> key, Reference<Integer> paramType) {
		return gsapi_enumerate_params(instance, iter, key, paramType);
	}

	public int add_control_path(int type, byte[] path) {
		return gsapi_add_control_path(instance, type, path);
	}

	public int add_control_path(int type, String path) {
		return gsapi_add_control_path(instance, type, path);
	}

	public int remove_control_path(int type, byte[] path) {
		return gsapi_remove_control_path(instance, type, path);
	}

	public int remove_control_path(int type, String path) {
		return gsapi_remove_control_path(instance, type, path);
	}

	public void purge_control_paths(int type) {
		gsapi_purge_control_paths(instance, type);
	}

	public void activate_path_control(boolean enable) {
		gsapi_activate_path_control(instance, enable);
	}

	public boolean is_path_control_active() {
		return gsapi_is_path_control_active(instance);
	}

	public GSParam<?>[] getParamArray() {
		List<GSParam<?>> params = new LinkedList<>();
		for (GSParam<?> param : this) {
			params.add(param);
		}
		return params.toArray(new GSParam<?>[params.size()]);
	}

	@Override
	public void finalize() {
		if (instance != GS_NULL) {
			exit();
			delete_instance();
		}
	}

	@Override
	public boolean equals(Object o) {
		if (o == null)
			return false;
		if (o == this)
			return true;
		if (o instanceof GSInstance) {
			GSInstance g = (GSInstance)o;
			return g.instance == instance && g.callerHandle == callerHandle;
		}
		return false;
	}

	@Override
	public String toString() {
		return "GSInstance[instance=0x" + Long.toHexString(instance) + "]";
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
			returnCode = enumerate_params(iterator, null, null);
			iterator.setValue(lastValue);
			return returnCode != 1;
		}

		@Override
		public GSParam<?> next() {
			if (returnCode != 0)
				throw new IllegalStateException("Reached end of iterator");
			returnCode = enumerate_params(iterator, key, valueType);
			if (returnCode < 0)
				throw new IllegalStateException("Failed to enumerate params");

			int code = get_param_once(key.getValue(), value, valueType.getValue());
			if (code != 0)
				throw new IllegalStateException("Failed to get param");
			return new GSParam<>(key.asString(), value.getValue());
		}

	}
}
