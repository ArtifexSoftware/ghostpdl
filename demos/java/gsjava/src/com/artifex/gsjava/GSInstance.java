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
 * Utility class to make Ghostscript calls easier by storing a
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
		if (instance != GS_NULL) {
			gsapi_delete_instance(instance);
			instance = GS_NULL;
		}
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

	public int runStringContinue(byte[] str, int length, int userErrors, IntReference pExitCode) {
		return gsapi_run_string_continue(instance, str, length, userErrors, pExitCode);
	}

	public int runStringContinue(String str, int length, int userErrors, IntReference pExitCode) {
		return gsapi_run_string_continue(instance, str, length, userErrors, pExitCode);
	}

	public int runString(byte[] str, int userErrors, IntReference pExitCode) {
		return gsapi_run_string(instance, str, userErrors, pExitCode);
	}

	public int runString(String str, int userErrors, IntReference pExitCode) {
		return gsapi_run_string(instance, str, userErrors, pExitCode);
	}

	public int runFile(byte[] fileName, int userErrors, IntReference pExitCode) {
		return gsapi_run_file(instance, fileName, userErrors, pExitCode);
	}

	public int runFile(String filename, int userErrors, IntReference pExitCode) {
		return gsapi_run_file(instance, filename, userErrors, pExitCode);
	}

	public int exit() {
		return gsapi_exit(instance);
	}

	@Override
	public void finalize() {
		if (instance != GS_NULL)
			exit();
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
