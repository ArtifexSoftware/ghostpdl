package com.artifex.gsjava;

import static com.artifex.gsjava.GSAPI.*;

import java.util.List;

import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.callbacks.ICalloutFunction;
import com.artifex.gsjava.callbacks.IPollFunction;
import com.artifex.gsjava.callbacks.IStdErrFunction;
import com.artifex.gsjava.callbacks.IStdInFunction;
import com.artifex.gsjava.callbacks.IStdOutFunction;
import com.artifex.gsjava.util.ByteArrayReference;
import com.artifex.gsjava.util.IntReference;
import com.artifex.gsjava.util.LongReference;

/**
 * Utility class to make Ghostscript calls easier by automatically storing a
 * Ghostscript instance and, optionally, a caller handle.
 *
 * @author Ethan Vrhel
 *
 */
public class GSInstance {

	private long instance;
	private long callerHandle;

	public GSInstance(long callerHandle) throws IllegalStateException {
		LongReference ref = new LongReference();
		int ret = gsapi_new_instance(ref, callerHandle);
		if (ret != GS_ERROR_OK)
			throw new IllegalStateException("Failed to create new instance: " + ret);
		this.instance = ref.value;
		this.callerHandle = callerHandle;
	}

	public GSInstance() throws IllegalStateException {
		this(GS_NULL);
	}

	public void deleteInstance() {
		gsapi_delete_instance(instance);
		instance = GS_NULL;
	}

	public int setStdio(IStdInFunction stdin, IStdOutFunction stdout, IStdErrFunction stderr) {
		return gsapi_set_stdio_with_handle(instance, stdin, stdout, stderr, callerHandle);
	}

	public int setPoll(IPollFunction pollfun) {
		return gsapi_set_poll_with_handle(instance, pollfun, callerHandle);
	}

	public int setDisplayCallback(DisplayCallback displaycallback) {
		return gsapi_set_display_callback(instance, displaycallback);
	}

	public int registerCallout(ICalloutFunction callout) {
		return gsapi_register_callout(instance, callout, callerHandle);
	}

	public void deregisterCallout(ICalloutFunction callout) {
		gsapi_deregister_callout(instance, callout, callerHandle);
	}

	public int setArgEncoding(int encoding) {
		return gsapi_set_arg_encoding(instance, encoding);
	}

	public int setDefaultDeviceList(byte[] list, int listlen) {
		return gsapi_set_default_device_list(instance, list, listlen);
	}

	public int getDefaultDeviceList(ByteArrayReference list, IntReference listlen) {
		return gsapi_get_default_device_list(instance, list, listlen);
	}

	public int initWithArgs(int argc, byte[][] argv) {
		return gsapi_init_with_args(instance, argc, argv);
	}

	public int initWithArgs(String[] argv) {
		return gsapi_init_with_args(instance, argv);
	}

	public int initWithArgs(List<String> argv) {
		return gsapi_init_with_args(instance, argv);
	}

	public int runStringBegin(int userErrors, IntReference pExitCode) {
		return gsapi_run_string_begin(instance, userErrors, pExitCode);
	}

	@Override
	public void finalize() {
		deleteInstance();
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
}
