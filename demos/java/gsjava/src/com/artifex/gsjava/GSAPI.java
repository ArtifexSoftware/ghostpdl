package com.artifex.gsjava;

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
 * Class which contains native bindings to Ghostscript via the JNI.
 *
 * @author Ethan Vrhel
 *
 */
public class GSAPI {

	static {
		registerLibraries();
	}

	/**
	 * Registers the needed native libraries.
	 */
	private static void registerLibraries() {
		System.loadLibrary("gs_jni");
	}

	/**
	 * NULL
	 */
	public static final long GS_NULL = 0L;

	/**
	 * Error codes
	 */
	public static final int GS_ERROR_OK = 0,
							GS_ERROR_UNKNOWNERROR = -1,
							GS_ERROR_DICTFULL = -2,
							GS_ERROR_DICTSTACKOVERFLOW = -3,
							GS_ERROR_DICTSTACKUNDERFLOW = -4,
							GS_ERROR_EXECSTACKOVERFLOW = -5,
							GS_ERROR_INTERRUPT = -6,
							GS_ERROR_INVALIDACCESS = -7,
							GS_ERROR_INVALIDEXIT = -8,
							GS_ERROR_INVALIDFILEACCESS = -9,
							GS_ERROR_INVALIDFONT = -10,
							GS_ERROR_INVALIDRESTORE = -11,
							GS_ERROR_IOERROR = -12,
							GS_ERROR_LIMITCHECK = -13,
							GS_ERROR_NOCURRENTPOINT = -14,
							GS_ERROR_RANGECHECK = -15,
							GS_ERROR_STACKOVERFLOW = -16,
							GS_ERROR_STACKUNDERFLOW = -17,
							GS_ERROR_SYNTAXERROR = -18,
							GS_ERROR_TIMEOUT = -19,
							GS_ERROR_TYPECHECK = -20,
							GS_ERROR_UNDEFINED = -21,
							GS_ERROR_UNDEFINEDFILENAME = -22,
							GS_ERROR_UNDEFINEDRESULT = -23,
							GS_ERROR_UNMATCHEDMARK = -24,
							GS_ERROR_VMERROR = -25;

	/**
	 * Error codes
	 */
	public static final int GS_ERROR_CONFIGURATION_ERROR = -26,
							GS_ERROR_UNDEFINEDRESOURCE = -27,
							GS_ERROR_UNREGISTERED = -28,
							GS_ERROR_INVALIDCONTEXT = -29,
							GS_ERROR_INVALID = -30;

	public static class Revision {
		public volatile byte[] product;
		public volatile byte[] copyright;
		public volatile long revision;
		public volatile long revisionDate;

		public Revision() {
			this.product = null;
			this.copyright = null;
			this.revision = 0L;
			this.revisionDate = 0L;
		}

		public String getProduct() {
			return new String(product);
		}

		public String getCopyright() {
			return new String(copyright);
		}
	}

	// Don't let this class be instantiated
	private GSAPI() { }

	public static native int gsapi_revision(GSAPI.Revision revision, int len);

	public static native int gsapi_new_instance(LongReference instance, long callerHandle);

	public static native void gsapi_delete_instance(long instance);

	public static native int gsapi_set_stdio_with_handle(long instance, IStdInFunction stdin,
			IStdOutFunction stdout, IStdErrFunction stderr, long callerHandle);

	public static native int gsapi_set_stdio(long instance, IStdInFunction stdin, IStdOutFunction stdout,
			IStdErrFunction stderr);

	public static native int gsapi_set_poll_with_handle(long instance, IPollFunction pollfun, long callerHandle);

	public static native int gsapi_set_poll(long instance, IPollFunction pollfun);

	public static native int gsapi_set_display_callback(long instance, DisplayCallback displayCallback);

	public static native int gsapi_register_callout(long instance, ICalloutFunction callout, long calloutHandle);

	public static native void gsapi_deregister_callout(long instance, ICalloutFunction callout, long calloutHandle);

	public static native int gsapi_set_arg_encoding(long instance, int encoding);

	public static native int gsapi_set_default_device_list(long instance, byte[] list, int listlen);

	public static native int gsapi_get_default_device_list(long instance, ByteArrayReference list, IntReference listlen);

	public static native int gsapi_init_with_args(long instance, int argc, ByteArrayReference argv);

	public static native int gsapi_run_string_begin(long instance, int userErrors, IntReference pExitCode);

	public static native int gsapi_run_string_continue(long instance, byte[] str, int length, int userErros,
			IntReference pExitCode);

	public static native int gsapi_run_string_end(long instance, int userErrors, IntReference pExitCode);

	public static native int gsapi_run_string_with_length(long instance, byte[] str, int length, int userErrors,
			IntReference pExitCode);

	public static native int gsapi_run_string(long instance, byte[] str, int userErrors, IntReference pExitCode);

	public static native int gsapi_run_file(long instance, byte[] fileName, int userErrors, IntReference pExitCode);

	public static native int gsapi_exit(long instance);
}
