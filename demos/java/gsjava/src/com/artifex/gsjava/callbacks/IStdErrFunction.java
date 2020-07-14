package com.artifex.gsjava.callbacks;

@FunctionalInterface
public interface IStdErrFunction {

	/**
	 * Called when something should be written to the standard error stream.
	 *
	 * @param callerHandle The caller handle.
	 * @param str The string to write.
	 * @param len The length of bytes to be written.
	 * @return The amount of bytes written, must be <code>len</code>.
	 */
	public int onStdErr(long callerHandle, byte[] str, int len);
}
