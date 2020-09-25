package com.artifex.gsviewer;

import com.artifex.gsjava.callbacks.IStdErrFunction;
import com.artifex.gsjava.callbacks.IStdInFunction;
import com.artifex.gsjava.callbacks.IStdOutFunction;

/**
 * Implementation of <code>IStdOutFunction</code>, <code>IStdErrFunction</code>, and
 * <code>IStdInFunction</code> to get IO data from Ghostscript.
 *
 * @author Ethan Vrhel
 *
 */
public class StdIO implements IStdOutFunction, IStdErrFunction, IStdInFunction {

	public static final boolean ENABLE_STD_OUT_IO = false;
	public static final boolean ENABLE_STD_ERR_IO = true;

	@Override
	public int onStdIn(long callerHandle, byte[] buf, int len) {
		return len;
	}

	@Override
	public int onStdErr(long callerHandle, byte[] str, int len) {
		if (ENABLE_STD_ERR_IO)
			System.err.println(new String(str));
		return len;
	}

	@Override
	public int onStdOut(long callerHandle, byte[] str, int len) {
		if (ENABLE_STD_OUT_IO)
			System.out.println(new String(str));
		return len;
	}

}
