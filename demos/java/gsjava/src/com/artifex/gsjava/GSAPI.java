package com.artifex.gsjava;

import java.util.List;

import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.callbacks.ICalloutFunction;
import com.artifex.gsjava.callbacks.IPollFunction;
import com.artifex.gsjava.callbacks.IStdErrFunction;
import com.artifex.gsjava.callbacks.IStdInFunction;
import com.artifex.gsjava.callbacks.IStdOutFunction;
import com.artifex.gsjava.util.Reference;
import com.artifex.gsjava.util.StringUtil;

import java.io.File;
import com.artifex.gsjava.util.BytePointer;

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
		try {
			// Try loading normally
			System.loadLibrary("gs_jni");
		} catch (UnsatisfiedLinkError e) {
			// Load using absolute paths
			if (System.getProperty("os.name").equalsIgnoreCase("Linux")) {
				// Load on Linux
				File libgpdl = new File("libgpdl.so");
				System.load(libgpdl.getAbsolutePath());
				File gsjni = new File("gs_jni.so");
				System.load(gsjni.getAbsolutePath());
			} else if (System.getProperty("os.name").equalsIgnoreCase("Mac OS X")) {
				// Load on Mac
				File libgpdl = new File("libgpdl.dylib");
				System.load(libgpdl.getAbsolutePath());
				File gsjni = new File("gs_jni.dylib");
				System.load(gsjni.getAbsolutePath());
			} else {
				throw e;
			}
		}
	}

	/**
	 * NULL
	 */
	public static final long GS_NULL = 0L;

	/**
	 * Level 1 error codes
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
	 * Level 2 error codes
	 */
	public static final int GS_ERROR_CONFIGURATION_ERROR = -26,
							GS_ERROR_UNDEFINEDRESOURCE = -27,
							GS_ERROR_UNREGISTERED = -28,
							GS_ERROR_INVALIDCONTEXT = -29,
							GS_ERROR_INVALID = -30;

	/**
	 * Psuedo-errors used internally
	 */
	public static final int GS_ERROR_HIT_DETECTED = -59,
							GS_ERROR_FATAL = -100;

	public static final int GS_COLORS_NATIVE = (1 << 0),
							GS_COLORS_GRAY = (1 << 1),
							GS_COLORS_RGB = (1 << 2),
							GS_COLORS_CMYK = (1 << 3),
							GS_DISPLAY_COLORS_SEPARATION = (1 << 19);

	public static final long GS_DISPLAY_COLORS_MASK = 0x8000fL;

	public static final int GS_DISPLAY_ALPHA_NONE = (0 << 4),
							GS_DISPLAY_ALPHA_FIRST = (1 << 4),
							GS_DISPLAY_ALPHA_LAST = (1 << 5),
							GS_DISPLAY_UNUSED_FIRST = (1 << 6),
							GS_DISPLAY_UNUSED_LAST = (1 << 7);

	public static final long GS_DISPLAY_ALPHA_MASK = 0x00f0L;

	public static final int GS_DISPLAY_DEPTH_1 = (1 << 8),
							GS_DISPLAY_DEPTH_2 = (1 << 9),
							GS_DISPLAY_DEPTH_4 = (1 << 10),
							GS_DISPLAY_DEPTH_8 = (1 << 11),
							GS_DISPLAY_DEPTH_12 = (1 << 12),
							GS_DISPLAY_DEPTH_16 = (1 << 3);

	public static final long GS_DISPLAY_DEPTH_MASK = 0xff00L;

	public static final int GS_DISPLAY_BIGENDIAN = (0 << 16),
							GS_DISPLAY_LITTLEENDIAN = (1 << 16);

	public static final long GS_DISPLAY_ENDIAN_MASK = 0x00010000L;

	public static final int GS_DISPLAY_TOPFIRST = (0 << 17),
							GS_DISPLAY_BOTTOMFIRST = (1 << 17);

	public static final long GS_DISPLAY_FIRSTROW_MASK = 0x00020000L;

	public static final int GS_SPT_INVALID = -1,
							GS_SPT_NULL = 0,
							GS_SPT_BOOL = 1,
							GS_SPT_INT = 2,
							GS_SPT_FLOAT = 3,
							GS_SPT_NAME = 4,
							GS_SPT_STRING = 5,
							GS_SPT_LONG = 6,
							GS_SPT_I64 = 7,
							GS_SPT_SIZE_T = 8,
							GS_SPT_PARSED = 9,
							GS_SPT_MORE_TO_COME = 1 << 31;

	/**
	 * Class used to store version information about Ghostscript.
	 *
	 * @author Ethan Vrhel
	 *
	 */
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

		/**
		 * Returns the product information as a String.
		 *
		 * @return The product information.
		 */
		public String getProduct() {
			return new String(product);
		}

		/**
		 * Returns the copyright information as a String.
		 *
		 * @return The copyright information.
		 */
		public String getCopyright() {
			return new String(copyright);
		}
	}

	// Don't let this class be instantiated
	private GSAPI() { }

	public static native int gsapi_revision(GSAPI.Revision revision, int len);

	public static native int gsapi_new_instance(Reference<Long> instance, long callerHandle);

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

	public static native int gsapi_get_default_device_list(long instance, Reference<byte[]> list, Reference<Integer> listlen);

	public static native int gsapi_init_with_args(long instance, int argc, byte[][] argv);

	public static native int gsapi_run_string_begin(long instance, int userErrors, Reference<Integer> pExitCode);

	public static native int gsapi_run_string_continue(long instance, byte[] str, int length, int userErrors,
			Reference<Integer> pExitCode);

	public static native int gsapi_run_string_end(long instance, int userErrors, Reference<Integer> pExitCode);

	public static native int gsapi_run_string_with_length(long instance, byte[] str, int length, int userErrors,
			Reference<Integer> pExitCode);

	public static native int gsapi_run_string(long instance, byte[] str, int userErrors, Reference<Integer> pExitCode);

	public static native int gsapi_run_file(long instance, byte[] fileName, int userErrors, Reference<Integer> pExitCode);

	public static native int gsapi_exit(long instance);

	public static native int gsapi_set_param(long instance, byte[] param, Object value, int paramType);

	public static native int gsapi_get_param(long instance, byte[] param, long value, int paramType);

	public static native int gsapi_get_param_once(long instance, byte[] param, Reference<?> value, int paramType);

	public static native int gsapi_enumerate_params(long instance, Reference<Long> iter, Reference<byte[]> key, Reference<Integer> paramType);

	public static native int gsapi_add_control_path(long instance, int type, byte[] path);

	public static native int gsapi_remove_control_path(long instance, int type, byte[] path);

	public static native void gsapi_purge_control_paths(long instance, int type);

	public static native void gsapi_activate_path_control(long instance, boolean enable);

	public static native boolean gsapi_is_path_control_active(long instance);

	// Utility methods to make calling some native methods easier

	public static int gsapi_init_with_args(long instance, String[] argv) {
		return gsapi_init_with_args(instance, argv.length, StringUtil.to2DByteArray(argv));
	}

	public static int gsapi_init_with_args(long instance, List<String> argv) {
		return gsapi_init_with_args(instance, argv.toArray(new String[argv.size()]));
	}

	public static int gsapi_run_string_continue(long instance, String str, int length, int userErrors,
			Reference<Integer> pExitCode) {
		return gsapi_run_string_continue(instance, StringUtil.toNullTerminatedByteArray(str.substring(0, length)),
				length, userErrors, pExitCode);
	}

	public static int gsapi_run_string_with_length(long instance, String str, int length, int userErrors,
			Reference<Integer> pExitCode) {
		return gsapi_run_string_with_length(instance, StringUtil.toNullTerminatedByteArray(str.substring(0, length)),
				length, userErrors, pExitCode);
	}

	public static int gsapi_run_string(long instance, String str, int userErrors, Reference<Integer> pExitCode) {
		return gsapi_run_string(instance, StringUtil.toNullTerminatedByteArray(str), userErrors, pExitCode);
	}

	public static int gsapi_run_file(long instance, String fileName, int userErrors, Reference<Integer> pExitCode) {
		return gsapi_run_file(instance, StringUtil.toNullTerminatedByteArray(fileName), userErrors, pExitCode);
	}

	public static int gsapi_set_param(long instance, String param, String value, int paramType) {
		return gsapi_set_param(instance, StringUtil.toNullTerminatedByteArray(param),
				StringUtil.toNullTerminatedByteArray(value), paramType);
	}

	public static int gsapi_set_param(long instance, String param, Object value, int paramType) {
		return gsapi_set_param(instance, StringUtil.toNullTerminatedByteArray(param), value, paramType);
	}

	public static int gsapi_get_param(long instance, String param, long value, int paramType) {
		return gsapi_get_param(instance, StringUtil.toNullTerminatedByteArray(param), value, paramType);
	}

	public static int gsapi_get_param_once(long instance, String param, Reference<?> value, int paramType) {
		return gsapi_get_param_once(instance, StringUtil.toNullTerminatedByteArray(param), value, paramType);
	}

	public static int gsapi_add_control_path(long instance, int type, String path) {
		return gsapi_add_control_path(instance, type, StringUtil.toNullTerminatedByteArray(path));
	}

	public static int gsapi_remove_control_path(long instance, int type, String path) {
		return gsapi_remove_control_path(instance, type, StringUtil.toNullTerminatedByteArray(path));
	}
}
