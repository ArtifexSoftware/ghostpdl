package com.artifex.gsjava;

import com.artifex.gsjava.callbacks.DisplayCallback;
import com.artifex.gsjava.callbacks.IPollFunction;
import com.artifex.gsjava.callbacks.IStdErrFunction;
import com.artifex.gsjava.callbacks.IStdInFunction;
import com.artifex.gsjava.callbacks.IStdOutFunction;

public class GSAPI {

	public static final long GS_NULL = 0;

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

	public static final int GS_ERROR_CONFIGURATION_ERROR = -26,
							GS_ERROR_UNDEFINEDRESOURCE = -27,
							GS_ERROR_UNREGISTERED = -28,
							GS_ERROR_INVALIDCONTEXT = -29,
							GS_ERROR_INVALID = -30;

	private GSAPI() { }

	public static native int gsapi_revision(int len);

	public static native int gsapi_new_instance(LongReference instance, long callerHandle);

	public static native void gsapi_delete_instance(long instance);

	public static native int gsapi_set_stdio_with_handle(long instance, IStdInFunction stdin,
			IStdOutFunction stdout, IStdErrFunction stderr, long callerHandle);

	public static native int gsapi_set_stdio(long instance, IStdInFunction stdin, IStdOutFunction stdout,
			IStdErrFunction stderr, long callerHandle);

	public static native int gsapi_set_poll_with_handle(long instance, IPollFunction pollfun, long callerHandle);

	public static native int gsapi_set_poll(long instance, IPollFunction pollfun);

	public static native int gsapi_set_display_callback(long instance, DisplayCallback displayCallback);
}
