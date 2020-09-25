package com.artifex.gsjava.callbacks;

@FunctionalInterface
public interface IStdInFunction {

	/**
	 *
	 *
	 * @param callerHandle The caller handle.
	 * @param buf A string.
	 * @param len The number of bytes to read.
	 * @return The number of bytes read, must be <code>len</code>/
	 */
	public int onStdIn(long callerHandle, byte[] buf, int len);
}
